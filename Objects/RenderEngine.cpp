#include "RenderEngine.h"
#include "Settings.h"
#include "GLSL2HLSL.h"
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
				if (m_objects->IsRenderTexture(objs[i])) {
					ed::RenderTextureObject* rtObj = m_objects->GetRenderTexture(objs[i]);
					if (rtObj != nullptr && rtObj->FixedSize.x == -1)
						m_objects->ResizeRenderTexture(objs[i], rtObj->CalculateSize(width, height));
				}
			}
		}

		// clear main rt only once
		ml::Color oldWndClrColor = m_wnd->GetClearColor();
		m_wnd->SetClearColor(Settings::Instance().Project.ClearColor);
		m_rt.Clear();
		m_rt.ClearDepthStencil(1.0f, 0);
		m_wnd->SetClearColor(oldWndClrColor);

		// bind default sampler state
		m_sampler.BindVS(0);
		m_sampler.BindPS(0);
		m_sampler.BindGS(0);

		// cache elements
		m_cache();

		auto& itemVarValues = GetItemVariableValues();
		ml::RenderTexture *previousRT[9], *currentRT[9]; // dont clear the render target if we use it two times in a row
		ID3D11DepthStencilView* previousDSV = nullptr;

		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* it = m_items[i];
			pipe::ShaderPass* data = (pipe::ShaderPass*)it->Data;
			std::vector<ml::ShaderResourceView*> srvs = m_objects->GetBindList(m_items[i]);

			// bind RTs
			int rtCount = 0;
			ID3D11DepthStencilView* depthView = nullptr;
			for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
				if (data->RenderTexture[i][0] == 0) {
					currentRT[i] = nullptr;
					break;
				}

				char rtName[64];
				strcpy(rtName, data->RenderTexture[i]);

				ml::RenderTexture* rt = nullptr;
				DirectX::XMINT2 rtSize = DirectX::XMINT2(width, height);

				if (strcmp(rtName, "Window") == 0) {
					// update viewport value
					SystemVariableManager::Instance().SetViewportSize(rtSize.x, rtSize.y);

					rt = &m_rt;

					if (depthView == nullptr)
						depthView = rt->GetDepthStencilView();
				}
				else {
					ed::RenderTextureObject* rtObject = m_objects->GetRenderTexture(rtName);
					ml::Color oldClearClr = m_wnd->GetClearColor();

					rtSize = rtObject->CalculateSize(width, height);
					rt = currentRT[i] = rtObject->RT;

					// update viewport value
					SystemVariableManager::Instance().SetViewportSize(rtSize.x, rtSize.y);

					// set viewport and cache old viewport
					D3D11_VIEWPORT viewport;
					viewport.TopLeftX = viewport.TopLeftY = 0;
					viewport.Height = rtSize.y;
					viewport.Width = rtSize.x;
					viewport.MinDepth = 0.0f;
					viewport.MaxDepth = 1.0f;
					m_wnd->GetDeviceContext()->RSSetViewports(1, &viewport);

					// clear and bind rt (only if not used in last shader pass)
					bool usedPreviously = false;
					for (int j = 0; j < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; j++)
						if (previousRT[j] == rt) {
							usedPreviously = true;
							break;
						}
					if (!usedPreviously) {
						m_wnd->SetClearColor(rtObject->ClearColor);
						rt->Clear();
						m_wnd->SetClearColor(oldClearClr);
					}

					if (depthView == nullptr) {
						depthView = rt->GetDepthStencilView();

						if (depthView != previousDSV)
							m_wnd->GetDeviceContext()->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
					}
					previousDSV = depthView;
				}

				// set viewport
				D3D11_VIEWPORT viewport;
				viewport.TopLeftX = viewport.TopLeftY = 0;
				viewport.Height = rtSize.y;
				viewport.Width = rtSize.x;
				viewport.MinDepth = 0.0f;
				viewport.MaxDepth = 1.0f;
				m_wnd->GetDeviceContext()->RSSetViewports(1, &viewport);

				// add render target
				m_views[rtCount] = rt->GetView();
				rtCount++;
			}
			for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
				previousRT[i] = currentRT[i];

			m_wnd->GetDeviceContext()->OMSetRenderTargets(rtCount, m_views, depthView);

			// bind shader resource views
			for (int j = 0; j < srvs.size(); j++) {
				srvs[j]->BindPS(j);
				srvs[j]->BindVS(j);
				if (data->GSUsed) srvs[j]->BindGS(j);
			}

			// bind shaders and their variables
			m_wnd->SetInputLayout(*m_vsLayout[i]);

			for (int j = 0; j < CONSTANT_BUFFER_SLOTS; j++) {
				if (data->VSVariables.IsSlotUsed(j))
					data->VSVariables.GetSlot(j).BindVS(j);
				if (data->PSVariables.IsSlotUsed(j))
					data->PSVariables.GetSlot(j).BindPS(j);
				if (data->GSUsed && data->GSVariables.IsSlotUsed(j))
					data->GSVariables.GetSlot(j).BindGS(j);
			}

			if (m_msgs->GetGroupWarningMsgCount(it->Name) > 0)
				m_msgs->ClearGroup(it->Name, (int)ed::MessageStack::Type::Warning);

			if (m_vs[i]->InputSignature == nullptr) {
				m_msgs->Add(ed::MessageStack::Type::Warning, it->Name, "Vertex shader/input layout not loaded - skipping the shader.");
				continue;
			}

			m_vs[i]->Bind();
			if (data->GSUsed && m_gs[i] != nullptr) m_gs[i]->Bind();
			m_ps[i]->Bind();

			// bind default states for each shader pass
			DefaultState::Instance().Bind();

			// render pipeline items
			for (int j = 0; j < data->Items.size(); j++) {
				PipelineItem* item = data->Items[j];

				SystemVariableManager::Instance().SetPicked(false);

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

					SystemVariableManager::Instance().SetPicked(m_pick == item);

					data->VSVariables.UpdateBuffers(m_wnd);
					data->PSVariables.UpdateBuffers(m_wnd);
					if (data->GSUsed) data->GSVariables.UpdateBuffers(m_wnd);

					m_wnd->SetTopology(geoData->Topology);
					geoData->Geometry.Draw();
				}
				else if (item->Type == PipelineItem::ItemType::OBJModel) {
					pipe::OBJModel* objData = reinterpret_cast<pipe::OBJModel*>(item->Data);

					SystemVariableManager::Instance().SetPicked(m_pick == item);

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
			for (int j = 0; j < srvs.size(); j++)
				m_wnd->RemoveShaderResource(j);

			// unbind geometry shader
			if (data->GSUsed) m_wnd->RemoveGeometryShader();
		}

		// bind default state after rendering everything
		DefaultState::Instance().Bind();

		// restore real render target view
		m_wnd->Bind();

		if (m_pickDist == std::numeric_limits<float>::infinity())
			m_pick = nullptr;
		if (m_pickAwaiting && m_pickHandle != nullptr)
			m_pickHandle(m_pick);
		m_pickAwaiting = false;
	}
	void RenderEngine::Recompile(const char * name)
	{
		m_msgs->BuildOccured = true;
		m_msgs->CurrentItem = name;

		int d3dCounter = 0;
		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* item = m_items[i];
			if (strcmp(item->Name, name) == 0) {
				ed::pipe::ShaderPass* shader = (ed::pipe::ShaderPass*)item->Data;

				m_msgs->ClearGroup(name);

				// bind input layout if needed 
				if (shader->VSInputLayout.GetInputElements().size() > 0) {
					ml::VertexInputLayout* inputLayout = m_vsLayout[i];
					inputLayout->GetInputElements().clear();

					auto inputLayoutElements = shader->VSInputLayout.GetInputElements();
					for (auto ilElement : inputLayoutElements)
						inputLayout->Add(ilElement.SemanticName, ilElement.SemanticIndex, ilElement.Format, ilElement.AlignedByteOffset, ilElement.InputSlot, ilElement.InputSlotClass, ilElement.InstanceDataStepRate);

					m_vs[i]->InputSignature = inputLayout;
					m_vs[i]->InputSignature->Reset();
				}
				else
					m_vs[i]->InputSignature = nullptr;

				std::string psContent = "", vsContent = "",
					vsEntry = shader->VSEntry,
					psEntry = shader->PSEntry;

				// pixel shader
				m_msgs->CurrentItemType = 1;
				if (!GLSL2HLSL::IsGLSL(shader->PSPath)) // HLSL
					psContent = m_project->LoadProjectFile(shader->PSPath);
				else {// GLSL
					psContent = ed::GLSL2HLSL::Transcompile(m_project->GetProjectPath(std::string(shader->PSPath)), 1, shader->PSEntry, m_wnd->GetLogger());
					psEntry = "main";
				}

				bool psCompiled = m_ps[i]->LoadFromMemory(*m_wnd, psContent.c_str(), psContent.size(), psEntry);

				// vertex shader
				m_msgs->CurrentItemType = 0;
				if (!GLSL2HLSL::IsGLSL(shader->VSPath)) // HLSL
					vsContent = m_project->LoadProjectFile(shader->VSPath);
				else { // GLSL
					vsContent = ed::GLSL2HLSL::Transcompile(m_project->GetProjectPath(std::string(shader->VSPath)), 0, shader->VSEntry, m_wnd->GetLogger());
					vsEntry = "main";
				}

				bool vsCompiled = m_vs[i]->LoadFromMemory(*m_wnd, vsContent.c_str(), vsContent.size(), vsEntry);

				// geometry shader
				bool gsCompiled = true;
				if (strlen(shader->GSPath) > 0 && strlen(shader->GSEntry) > 0) {
					if (m_gs[i] == nullptr)
						m_gs[i] = new ml::GeometryShader();

					std::string gsContent = "",
						gsEntry = shader->GSEntry;

					m_msgs->CurrentItemType = 2;
					if (!GLSL2HLSL::IsGLSL(shader->GSPath)) // HLSL
						gsContent = m_project->LoadProjectFile(shader->GSPath);
					else { // GLSL
						gsContent = ed::GLSL2HLSL::Transcompile(m_project->GetProjectPath(std::string(shader->GSPath)), 2, shader->GSEntry, m_wnd->GetLogger());
						gsEntry = "main";
					}

					gsCompiled = m_gs[i]->LoadFromMemory(*m_wnd, gsContent.c_str(), gsContent.size(), gsEntry);
				}

				if (!vsCompiled || !psCompiled || !gsCompiled) {
					m_msgs->Add(MessageStack::Type::Error, name, "Failed to compile the shader(s)");
				}
				else
					m_msgs->Add(MessageStack::Type::Message, name, "Compiled the shaders.");
			}
		}
	}
	void RenderEngine::Pick(float sx, float sy, std::function<void(PipelineItem*)> func)
	{
		m_pickAwaiting = true;
		m_pickDist = std::numeric_limits<float>::infinity();
		m_pickHandle = func;
		
		DirectX::XMMATRIX proj = SystemVariableManager::Instance().GetProjectionMatrix();
		DirectX::XMFLOAT4X4 proj4x4; DirectX::XMStoreFloat4x4(&proj4x4, proj);

		float vx = (+2.0f*sx/m_lastSize.x - 1.0f)/ proj4x4(0,0);
		float vy = (-2.0f*sy/m_lastSize.y + 1.0f)/ proj4x4(1,1);

		m_pickOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		m_pickDir = DirectX::XMVectorSet(vx, vy, 1.0f, 0.0f);

		DirectX::XMMATRIX view = SystemVariableManager::Instance().GetCamera()->GetMatrix();
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

		float myDist = std::numeric_limits<float>::infinity();
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

			ml::Bounds3D bounds = obj->Mesh.GetBounds();
			ml::OBJModel::Vertex* verts = obj->Mesh.GetVertexData();
			ml::UInt32 vertCount = obj->Mesh.GetVertexCount();

			if (obj->OnlyGroup) {
				verts = obj->Mesh.GetGroupVertices(obj->GroupName);
				vertCount = obj->Mesh.GetGroupVertexCount(obj->GroupName);
				bounds = obj->Mesh.GetGroupBounds(obj->GroupName);

				if (verts == nullptr) {
					verts = obj->Mesh.GetObjectVertices(obj->GroupName);
					vertCount = obj->Mesh.GetObjectVertexCount(obj->GroupName);
					bounds = obj->Mesh.GetObjectBounds(obj->GroupName);
				}
			}

			DirectX::BoundingBox iBox;
			iBox.Center = bounds.Center();
			iBox.Extents = DirectX::XMFLOAT3(
				std::abs(bounds.Max.x - bounds.Min.x) / 2,
				std::abs(bounds.Max.y - bounds.Min.y) / 2,
				std::abs(bounds.Max.z - bounds.Min.z) / 2
			);

			float triDist = std::numeric_limits<float>::infinity();
			if (iBox.Intersects(rayOrigin, rayDir, triDist)) {
				if (vertCount % 3 == 0 && triDist < m_pickDist) { // optimization: check if bounding box is closer than selected object
					for (int i = 0; i < vertCount; i += 3) {
						DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&verts[i+0].Position);
						DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&verts[i+1].Position);
						DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&verts[i+2].Position);

						if (DirectX::TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, triDist)) {
							if (triDist < myDist)
								myDist = triDist;
						}
					}
				}
			}
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
		for (int i = 0; i < m_vsLayout.size(); i++)
			delete m_vsLayout[i];
		m_vs.clear();
		m_ps.clear();
		m_gs.clear();
		m_items.clear();
		m_vsLayout.clear();
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
				std::vector<D3D11_INPUT_ELEMENT_DESC> inputEls = data->VSInputLayout.GetInputElements();

				ml::VertexShader* vShader = new ml::VertexShader();
				ml::PixelShader* pShader = new ml::PixelShader();

				m_msgs->CurrentItem = items[i]->Name;

				m_vsLayout.insert(m_vsLayout.begin() + i, new ml::VertexInputLayout());
				ml::VertexInputLayout* inputLayout = m_vsLayout[i];
				auto inputLayoutElements = data->VSInputLayout.GetInputElements();
				for (auto ilElement : inputLayoutElements)
					inputLayout->Add(ilElement.SemanticName, ilElement.SemanticIndex, ilElement.Format, ilElement.AlignedByteOffset, ilElement.InputSlot, ilElement.InputSlotClass, ilElement.InstanceDataStepRate);

				// bind the input layout
				data->VSInputLayout.Reset();
				if (data->VSInputLayout.GetInputElements().size() > 0)
					vShader->InputSignature = inputLayout;
				else
					vShader->InputSignature = nullptr;

				std::string psContent = "", vsContent = "",
					vsEntry = data->VSEntry,
					psEntry = data->PSEntry;

				// vertex shader
				m_msgs->CurrentItemType = 0;
				if (!GLSL2HLSL::IsGLSL(data->VSPath)) // HLSL
					vsContent = m_project->LoadProjectFile(data->VSPath);
				else { // GLSL
					vsContent = ed::GLSL2HLSL::Transcompile(m_project->GetProjectPath(std::string(data->VSPath)), 0, data->VSEntry, m_wnd->GetLogger());
					vsEntry = "main";
				}

				bool vsCompiled = vShader->LoadFromMemory(*m_wnd, vsContent.c_str(), vsContent.size(), vsEntry);

				// pixel shader
				m_msgs->CurrentItemType = 1;
				if (!GLSL2HLSL::IsGLSL(data->PSPath)) // HLSL
					psContent = m_project->LoadProjectFile(data->PSPath);
				else { // GLSL
					psContent = ed::GLSL2HLSL::Transcompile(m_project->GetProjectPath(std::string(data->PSPath)), 1, data->PSEntry, m_wnd->GetLogger());
					psEntry = "main";
				}

				bool psCompiled = pShader->LoadFromMemory(*m_wnd, psContent.c_str(), psContent.size(), psEntry);

				// geometry shader
				bool gsCompiled = true;
				if (strlen(data->GSEntry) > 0 && strlen(data->GSPath) > 0) {
					std::string gsContent = "", gsEntry = data->GSEntry;
					m_msgs->CurrentItemType = 2;
					if (!GLSL2HLSL::IsGLSL(data->GSPath)) // HLSL
						gsContent = m_project->LoadProjectFile(data->GSPath);
					else { // GLSL
						gsContent = ed::GLSL2HLSL::Transcompile(m_project->GetProjectPath(std::string(data->GSPath)), 2, data->GSEntry, m_wnd->GetLogger());
						gsEntry = "main";
					}

					ml::GeometryShader* gShader = new ml::GeometryShader();
					gsCompiled = gShader->LoadFromMemory(*m_wnd, gsContent.c_str(), gsContent.size(), gsEntry);
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
				delete m_vsLayout[i];

				if (m_gs[i] != nullptr)
					delete m_gs[i];
				
				m_vs.erase(m_vs.begin() + i);
				m_vsLayout.erase(m_vsLayout.begin() + i);
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
						ml::VertexInputLayout* vsLayoutCopy = m_vsLayout[i];

						m_vs.erase(m_vs.begin() + i);
						m_vs.insert(m_vs.begin() + dest, vsCopy);

						m_vsLayout.erase(m_vsLayout.begin() + i);
						m_vsLayout.insert(m_vsLayout.begin() + dest, vsLayoutCopy);

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