#include "RenderEngine.h"
#include "DefaultState.h"
#include "ObjectManager.h"
#include "PipelineManager.h"
#include "SystemVariableManager.h"
#include <MoonLight/Base/ConstantBuffer.h>
#include <DirectXCollision.h>

namespace ed
{
	RenderEngine::RenderEngine(ml::Window* wnd, PipelineManager * pipeline, ObjectManager* objects, ProjectParser* project, MessageStack* msgs) :
		m_pipeline(pipeline),
		m_objects(objects),
		m_project(project),
		m_msgs(msgs),
		m_wnd(wnd),
		m_lastSize(0, 0),
		m_pickAwaiting(false),
		m_pick(nullptr)
	{
		m_sampler.Create(*wnd);
	}
	RenderEngine::~RenderEngine()
	{
		FlushCache();
	}
	void RenderEngine::Render(int width, int height)
	{
		// recreate render texture if size is changed
		if (m_lastSize.x != width || m_lastSize.y != height) {
			m_lastSize = DirectX::XMINT2(width, height);
			m_rt.Create(*m_wnd, m_lastSize, ml::Resource::ShaderResource, true);

			m_rtView.Create(*m_wnd, m_rt);
		
			std::vector<std::string> objs = m_objects->GetObjects();
			for (int i = 0; i < objs.size(); i++) {
				ed::RenderTextureObject* rtObj = m_objects->GetRenderTexture(objs[i]);
				if (rtObj->FixedSize.x == -1)
					m_objects->ResizeRenderTexture(objs[i], rtObj->CalculateSize(width, height));
			}
		}

		// clear main rt only once
		m_rt.Clear();
		m_rt.ClearDepthStencil(1.0f, 0);

		// bind default sampler state
		m_sampler.BindVS(0);
		m_sampler.BindPS(0);
		m_sampler.BindGS(0);

		// cache elements
		m_cache();

		auto& itemVarValues = GetItemVariableValues();

		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* it = m_items[i];
			pipe::ShaderPass* data = (pipe::ShaderPass*)it->Data;
			std::vector<ml::ShaderResourceView*> srvs = m_objects->GetBindList(m_items[i]);

			if (strcmp(data->RenderTexture, "Window") == 0) {
				// update viewport value
				SystemVariableManager::Instance().SetViewportSize(width, height);

				// set viewport and cache old viewport
				D3D11_VIEWPORT viewport;
				viewport.TopLeftX = viewport.TopLeftY = 0;
				viewport.Height = height;
				viewport.Width = width;
				viewport.MinDepth = 0.0f;
				viewport.MaxDepth = 1.0f;
				m_wnd->GetDeviceContext()->RSSetViewports(1, &viewport);

				// bind window rt
				m_rt.Bind();
			}
			else {
				ed::RenderTextureObject* rt = m_objects->GetRenderTexture(data->RenderTexture);
				ml::Color oldClearClr = m_wnd->GetClearColor();
				DirectX::XMINT2 calcSize = rt->CalculateSize(width, height);

				// update viewport value
				SystemVariableManager::Instance().SetViewportSize(calcSize.x, calcSize.y);

				// set viewport and cache old viewport
				D3D11_VIEWPORT viewport;
				viewport.TopLeftX = viewport.TopLeftY = 0;
				viewport.Height = calcSize.y;
				viewport.Width = calcSize.x;
				viewport.MinDepth = 0.0f;
				viewport.MaxDepth = 1.0f;
				m_wnd->GetDeviceContext()->RSSetViewports(1, &viewport);

				// clear and bind rt
				m_wnd->SetClearColor(rt->ClearColor);
				rt->RT->Bind();
				rt->RT->Clear();
				rt->RT->ClearDepthStencil(1.0f, 0);
				m_wnd->SetClearColor(oldClearClr);
			}

			// bind shader resource views
			for (int i = 0; i < srvs.size(); i++) {
				srvs[i]->BindPS(i);
				srvs[i]->BindVS(i);
				if (data->GSUsed) srvs[i]->BindGS(i);
			}

			// bind shaders and their variables
			m_wnd->SetInputLayout(data->VSInputLayout);

			for (int j = 0; j < CONSTANT_BUFFER_SLOTS; j++) {
				if (data->VSVariables.IsSlotUsed(j))
					data->VSVariables.GetSlot(j).BindVS(j);
				if (data->PSVariables.IsSlotUsed(j))
					data->PSVariables.GetSlot(j).BindPS(j);
				if (data->GSUsed && data->GSVariables.IsSlotUsed(j))
					data->GSVariables.GetSlot(j).BindGS(j);
			}

			m_vs[i]->Bind();
			if (data->GSUsed && m_gs[i] != nullptr) m_gs[i]->Bind();
			m_ps[i]->Bind();

			// bind default states for each shader pass
			DefaultState::Instance().Bind();

			// render pipeline items
			for (int j = 0; j < data->Items.size(); j++) {
				PipelineItem* item = data->Items[j];

				if (m_pick == item) continue;

				// update the value for this element and check if the we picked it
				if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::OBJModel) {
					if (m_pickAwaiting) m_pickItem(item);
					for (int k = 0; k < itemVarValues.size(); k++)
						if (itemVarValues[k].Item == item)
							itemVarValues[k].Variable->Data = itemVarValues[k].NewValue->Data;
				}

				if (item->Type == PipelineItem::ItemType::Geometry) {
					pipe::GeometryItem* geoData = reinterpret_cast<pipe::GeometryItem*>(item->Data);

					if (geoData->Type == pipe::GeometryItem::Rectangle) {
						DirectX::XMFLOAT3 scaleRect(geoData->Scale.x * width, geoData->Scale.y * height, 1.0f);
						DirectX::XMFLOAT3 posRect(geoData->Position.x * width, geoData->Position.y * height, 1.0f);
						SystemVariableManager::Instance().SetGeometryTransform(scaleRect, geoData->Rotation, posRect);
					} else
						SystemVariableManager::Instance().SetGeometryTransform(geoData->Scale, geoData->Rotation, geoData->Position);
					data->VSVariables.UpdateBuffers(m_wnd);
					data->PSVariables.UpdateBuffers(m_wnd);
					if (data->GSUsed) data->GSVariables.UpdateBuffers(m_wnd);

					m_wnd->SetTopology(geoData->Topology);
					geoData->Geometry.Draw();
				}
				else if (item->Type == PipelineItem::ItemType::OBJModel) {
					pipe::OBJModel* objData = reinterpret_cast<pipe::OBJModel*>(item->Data);

					// TODO: model transform
					SystemVariableManager::Instance().SetGeometryTransform(objData->Scale, objData->Rotation, objData->Position);
					data->VSVariables.UpdateBuffers(m_wnd);
					data->PSVariables.UpdateBuffers(m_wnd);
					if (data->GSUsed) data->GSVariables.UpdateBuffers(m_wnd);

					m_wnd->SetTopology(ml::Topology::TriangleList);
					objData->Vertices.Bind();
					m_wnd->Draw(objData->VertCount, 0);
				}
				else if (item->Type == PipelineItem::ItemType::BlendState) {
					pipe::BlendState* blend = reinterpret_cast<pipe::BlendState*>(item->Data);
					blend->State.Bind();
				}
				else if (item->Type == PipelineItem::ItemType::DepthStencilState) {
					pipe::DepthStencilState* state = reinterpret_cast<pipe::DepthStencilState*>(item->Data);
					state->State.Bind(state->StencilReference);
				}
				else if (item->Type == PipelineItem::ItemType::RasterizerState) {
					pipe::RasterizerState* state = reinterpret_cast<pipe::RasterizerState*>(item->Data);
					state->State.Bind();
				}

				// set the old value back
				if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::OBJModel)
					for (int k = 0; k < itemVarValues.size(); k++)
						if (itemVarValues[k].Item == item)
							itemVarValues[k].Variable->Data = itemVarValues[k].OldValue;

			}

			// unbind shader resource views
			for (int i = 0; i < srvs.size(); i++)
				m_wnd->RemoveShaderResource(i);

			// unbind geometry shader
			if (data->GSUsed) m_wnd->RemoveGeometryShader();
		}

		// restore real render target view
		m_wnd->Bind();

		m_pickAwaiting = false;
		if (m_pickDist == std::numeric_limits<float>::infinity())
			m_pick = nullptr;
	}
	void RenderEngine::Recompile(const char * name)
	{
		int d3dCounter = 0;
		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* item = m_items[i];
			if (strcmp(item->Name, name) == 0) {
				ed::pipe::ShaderPass* shader = (ed::pipe::ShaderPass*)item->Data;

				// bind input layout if needed 
				if (shader->VSInputLayout.GetInputElements().size() > 0) {
					m_vs[i]->InputSignature = &shader->VSInputLayout;
					m_vs[i]->InputSignature->Reset();
				} 
				else
					m_vs[i]->InputSignature = nullptr;

				std::string vsContent = m_project->LoadProjectFile(shader->VSPath);
				std::string psContent = m_project->LoadProjectFile(shader->PSPath);
				bool vsCompiled = m_vs[i]->LoadFromMemory(*m_wnd, vsContent.c_str(), vsContent.size(), shader->VSEntry);
				bool psCompiled = m_ps[i]->LoadFromMemory(*m_wnd, psContent.c_str(), psContent.size(), shader->PSEntry);

				bool gsCompiled = true;
				if (strlen(shader->GSPath) > 0 && strlen(shader->GSEntry) > 0) {
					if (m_gs[i] == nullptr)
						m_gs[i] = new ml::GeometryShader();

					std::string gsContent = m_project->LoadProjectFile(shader->GSPath);
					gsCompiled = m_gs[i]->LoadFromMemory(*m_wnd, gsContent.c_str(), gsContent.size(), shader->GSEntry);
				}

				if (!vsCompiled || !psCompiled || !gsCompiled)
					m_msgs->Add(MessageStack::Type::Error, item->Name, "Failed to compile the shader(s)");
				else
					m_msgs->ClearGroup(item->Name);
			}
		}
	}
	void RenderEngine::Pick(float sx, float sy)
	{
		m_pickAwaiting = true;
		m_pickDist = std::numeric_limits<float>::infinity();
		
		DirectX::XMMATRIX proj = SystemVariableManager::Instance().GetProjectionMatrix();
		DirectX::XMFLOAT4X4 proj4x4; DirectX::XMStoreFloat4x4(&proj4x4, proj);

		float vx = (+2.0f*sx/m_lastSize.x - 1.0f)/ proj4x4(0,0);
		float vy = (-2.0f*sy/m_lastSize.y + 1.0f)/ proj4x4(1,1);

		m_pickOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		m_pickDir = DirectX::XMVectorSet(vx, vy, 1.0f, 0.0f);

		DirectX::XMMATRIX view = SystemVariableManager::Instance().GetCamera().GetMatrix();
		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&XMMatrixDeterminant(view), view);

		m_pickOrigin = DirectX::XMVector3TransformCoord(m_pickOrigin, invView);
		m_pickDir = DirectX::XMVector3TransformNormal(m_pickDir, invView);
	}
	void RenderEngine::m_pickItem(PipelineItem* item)
	{
		DirectX::XMMATRIX world;
		if (item->Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem* geo = (pipe::GeometryItem*)item->Data;
			if (geo->Type == pipe::GeometryItem::GeometryType::Rectangle)
				return;

			world = DirectX::XMMatrixScaling(geo->Scale.x, geo->Scale.y, geo->Scale.z) *
				DirectX::XMMatrixRotationRollPitchYaw(geo->Rotation.x, geo->Rotation.y, geo->Rotation.z) *
				DirectX::XMMatrixTranslation(geo->Position.x, geo->Position.y, geo->Position.z);
		}
		else if (item->Type == PipelineItem::ItemType::OBJModel) {
			pipe::OBJModel* obj = (pipe::OBJModel*)item->Data;
			
			world = DirectX::XMMatrixScaling(obj->Scale.x, obj->Scale.y, obj->Scale.z) *
				DirectX::XMMatrixRotationRollPitchYaw(obj->Rotation.x, obj->Rotation.y, obj->Rotation.z) *
				DirectX::XMMatrixTranslation(obj->Position.x, obj->Position.y, obj->Position.z);
		}
		DirectX::XMMATRIX invWorld = DirectX::XMMatrixInverse(&XMMatrixDeterminant(world), world);
		
		DirectX::XMVECTOR rayOrigin = DirectX::XMVector3TransformCoord(m_pickOrigin, invWorld);
		DirectX::XMVECTOR rayDir = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(m_pickDir, invWorld));

		float myDist;
		if (item->Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem* geo = (pipe::GeometryItem*)item->Data;
			if (geo->Type == pipe::GeometryItem::GeometryType::Cube) {
				DirectX::BoundingBox iBox;
				iBox.Center = DirectX::XMFLOAT3(0, 0, 0);
				iBox.Extents = DirectX::XMFLOAT3(geo->Size.x / 2, geo->Size.y / 2, geo->Size.z / 2);

				if (!iBox.Intersects(rayOrigin, rayDir, myDist))
					myDist = std::numeric_limits<float>::infinity();
			}
			else if (geo->Type == pipe::GeometryItem::GeometryType::Triangle) {
				float size = geo->Size.x;
				float rightOffs = size / tan(DirectX::XMConvertToRadians(30));

				DirectX::XMVECTOR v0 = DirectX::XMVectorSet(0, size, 0, 0);
				DirectX::XMVECTOR v1 = DirectX::XMVectorSet(rightOffs, -size, 0, 0);
				DirectX::XMVECTOR v2 = DirectX::XMVectorSet(-rightOffs, -size, 0, 0);
				if (!DirectX::TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, myDist))
					myDist = std::numeric_limits<float>::infinity();
			}
			else if (geo->Type == pipe::GeometryItem::GeometryType::Sphere) {
				DirectX::BoundingSphere sphere;
				sphere.Center = DirectX::XMFLOAT3(0, 0, 0);
				sphere.Radius = geo->Size.x;

				if (!sphere.Intersects(rayOrigin, rayDir, myDist))
					myDist = std::numeric_limits<float>::infinity();
			}
			else if (geo->Type == pipe::GeometryItem::GeometryType::Plane) {
				DirectX::BoundingBox iBox;
				iBox.Center = DirectX::XMFLOAT3(0, 0, 0);
				iBox.Extents = DirectX::XMFLOAT3(geo->Size.x / 2, geo->Size.y / 2, 0.0001f);

				if (!iBox.Intersects(rayOrigin, rayDir, myDist))
					myDist = std::numeric_limits<float>::infinity();
			}
			else if (geo->Type == pipe::GeometryItem::GeometryType::Circle) {
				DirectX::BoundingBox iBox;
				iBox.Center = DirectX::XMFLOAT3(0, 0, 0);
				iBox.Extents = DirectX::XMFLOAT3(geo->Size.x, geo->Size.y, 0.0001f);

				if (!iBox.Intersects(rayOrigin, rayDir, myDist))
					myDist = std::numeric_limits<float>::infinity();
			}
		} else if (item->Type == PipelineItem::ItemType::OBJModel) {
			pipe::OBJModel* obj = (pipe::OBJModel*)item->Data;

			/* TODO: 1. AABB intersection check 2. triangle intersection check */
		}

		// did we actually pick sth that is closer?
		if (myDist < m_pickDist) {
			m_pickDist = myDist;
			m_pick = item;
		}
	}
	void RenderEngine::FlushCache()
	{
		for (int i = 0; i < m_vs.size(); i++)
			delete m_vs[i];
		for (int i = 0; i < m_ps.size(); i++)
			delete m_ps[i];
		for (int i = 0; i < m_gs.size(); i++)
			delete m_gs[i];
		m_vs.clear();
		m_ps.clear();
		m_gs.clear();
		m_items.clear();
	}
	void RenderEngine::m_cache()
	{
		// check for any changes
		std::vector<ed::PipelineItem*>& items = m_pipeline->GetList();

		// check if no major changes were made, if so dont cache for another 0.25s
		if (m_items.size() == items.size()) {
			if (m_cacheTimer.GetElapsedTime() > 0.5f)
				m_cacheTimer.Restart();
			else return;
		}

		// check if some item was added
		for (int i = 0; i < items.size(); i++) {
			bool found = false;
			for (int j = 0; j < m_items.size(); j++)
				if (items[i]->Data == m_items[j]->Data) {
					found = true;
					break;
				}

			if (!found) {
				m_items.insert(m_items.begin() + i, items[i]);

				/*
					ITEM CACHING
				*/

				ed::pipe::ShaderPass* data = reinterpret_cast<ed::pipe::ShaderPass*>(items[i]->Data);

				ml::VertexShader* vShader = new ml::VertexShader();
				ml::PixelShader* pShader = new ml::PixelShader();

				// bind the input layout
				data->VSInputLayout.Reset();
				if (data->VSInputLayout.GetInputElements().size() > 0)
					vShader->InputSignature = &data->VSInputLayout;
				else
					vShader->InputSignature = nullptr;

				std::string vsContent = m_project->LoadProjectFile(data->VSPath);
				std::string psContent = m_project->LoadProjectFile(data->PSPath);
				bool vsCompiled = vShader->LoadFromMemory(*m_wnd, vsContent.c_str(), vsContent.size(), data->VSEntry);
				bool psCompiled = pShader->LoadFromMemory(*m_wnd, psContent.c_str(), psContent.size(), data->PSEntry);
				bool gsCompiled = true;

				if (strlen(data->GSEntry) > 0 && strlen(data->GSPath) > 0) {
					std::string gsContent = m_project->LoadProjectFile(data->GSPath);
					ml::GeometryShader* gShader = new ml::GeometryShader();

					gsCompiled = gShader->LoadFromMemory(*m_wnd, gsContent.c_str(), gsContent.size(), data->GSEntry);
					m_gs.insert(m_gs.begin() + i, gShader);
				}
				else m_gs.insert(m_gs.begin() + i, nullptr);

				if (!vsCompiled || !psCompiled || !gsCompiled)
					m_msgs->Add(MessageStack::Type::Error, items[i]->Name, "Failed to compile the shader");
				else
					m_msgs->ClearGroup(items[i]->Name);

				m_vs.insert(m_vs.begin() + i, vShader);
				m_ps.insert(m_ps.begin() + i, pShader);
			}
		}

		// check if some item was removed
		for (int i = 0; i < m_items.size(); i++) {
			bool found = false;
			for (int j = 0; j < items.size(); j++)
				if (items[j]->Data == m_items[i]->Data) {
					found = true;
					break;
				}

			if (!found) {
				delete m_vs[i];
				delete m_ps[i];

				if (m_gs[i] != nullptr)
					delete m_gs[i];
				
				m_vs.erase(m_vs.begin() + i);
				m_ps.erase(m_ps.begin() + i);
				m_gs.erase(m_gs.begin() + i);

				m_items.erase(m_items.begin() + i);
			}
		}

		// check if the order of the items changed
		for (int i = 0; i < m_items.size(); i++) {
			// two items at the same position dont match
			if (items[i]->Data != m_items[i]->Data) {
				// find the real position from original list
				for (int j = 0; j < items.size(); j++) {
					// we found the original position so move the item
					if (items[j]->Data == m_items[i]->Data) {
						int dest = j > i ? (j - 1) : j;
						m_items.erase(m_items.begin() + i, m_items.begin() + i + 1);
						m_items.insert(m_items.begin() + dest, items[j]);

						ml::VertexShader* vsCopy = m_vs[i];
						ml::PixelShader* psCopy = m_ps[i];
						ml::GeometryShader* gsCopy = m_gs[i];

						m_vs.erase(m_vs.begin() + i);
						m_vs.insert(m_vs.begin() + dest, vsCopy);

						m_ps.erase(m_ps.begin() + i);
						m_ps.insert(m_ps.begin() + dest, psCopy);

						m_gs.erase(m_gs.begin() + i);
						m_gs.insert(m_gs.begin() + dest, gsCopy);
					}
				}
			}
		}
	}
}