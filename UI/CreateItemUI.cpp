#include "CreateItemUI.h"
#include "UIHelper.h"
#include "../Objects/Names.h"
#include "../Objects/Settings.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <MoonLight/Base/GeometryFactory.h>
#include <MoonLight/Model/OBJModel.h>

#define PATH_SPACE_LEFT -40 * Settings::Instance().DPIScale

namespace ed
{
	void CreateItemUI::OnEvent(const ml::Event & e)
	{
	}
	void CreateItemUI::Update(float delta)
	{
		int colWidth = 150;
		if (m_item.Type == PipelineItem::ItemType::DepthStencilState || m_item.Type == PipelineItem::ItemType::BlendState)
			colWidth = 200;

		ImGui::Columns(2, 0, false);
		ImGui::SetColumnWidth(0, colWidth);

		ImGui::Text("Name:");
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		ImGui::InputText("##cui_name", m_item.Name, PIPELINE_ITEM_NAME_LENGTH);
		ImGui::NextColumn();

		if (m_item.Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem* data = (pipe::GeometryItem*)m_item.Data;

			// geo type
			ImGui::Text("Geometry type:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Combo("##cui_geotype", reinterpret_cast<int*>(&data->Type), GEOMETRY_NAMES, ARRAYSIZE(GEOMETRY_NAMES));
			ImGui::NextColumn();

			//size
			ImGui::Text("Size:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateInputFloat3("##cui_geosize", data->Size);
			ImGui::NextColumn();
		}
		else if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* data = (pipe::ShaderPass*)m_item.Data;

			// vs path
			ImGui::Text("Vertex shader path:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::InputText("##cui_spvspath", data->VSPath, MAX_PATH);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_spvspath", ImVec2(-1, 0))) {
				std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_data->GetOwner()->GetWindowHandle()));
				if (file.size() != 0) {
					strcpy(data->VSPath, file.c_str());

					if (m_data->Parser.FileExists(file))
						m_data->Messages.ClearGroup(m_item.Name);
					else
						m_data->Messages.Add(ed::MessageStack::Type::Error, m_item.Name, "Vertex shader file doesnt exist");
				}
			}
			ImGui::NextColumn();

			// vs entry
			ImGui::Text("Vertex shader entry:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputText("##cui_spvsentry", data->VSEntry, 32);
			ImGui::NextColumn();

			// ps path
			ImGui::Text("Pixel shader path:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::InputText("##cui_sppspath", data->PSPath, MAX_PATH);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_sppspath", ImVec2(-1, 0))) {
				std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_data->GetOwner()->GetWindowHandle()));
				if (file.size() != 0) {
					strcpy(data->PSPath, file.c_str());

					if (m_data->Parser.FileExists(file))
						m_data->Messages.ClearGroup(m_item.Name);
					else
						m_data->Messages.Add(ed::MessageStack::Type::Error, m_item.Name, "Pixel shader file doesnt exist");
				}
			}
			ImGui::NextColumn();

			// ps entry
			ImGui::Text("Pixel shader entry:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputText("##cui_sppsentry", data->PSEntry, 32);
			ImGui::NextColumn();

			// gs used
			ImGui::Text("Use geometry shader:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_spgsuse", &data->GSUsed);
			ImGui::NextColumn();

			if (!data->GSUsed) ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

			// gs path
			ImGui::Text("Geometry shader path:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::InputText("##cui_spgspath", data->GSPath, MAX_PATH);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_spgspath", ImVec2(-1, 0))) {
				std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_data->GetOwner()->GetWindowHandle()));
				if (file.size() != 0) {
					strcpy(data->GSPath, file.c_str());

					if (m_data->Parser.FileExists(file))
						m_data->Messages.ClearGroup(m_item.Name);
					else
						m_data->Messages.Add(ed::MessageStack::Type::Error, m_item.Name, "Geometry shader file doesnt exist");
				}
			}
			ImGui::NextColumn();

			// gs entry
			ImGui::Text("Geometry shader entry:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputText("##cui_spgsentry", data->GSEntry, 32);
			ImGui::NextColumn();

			if (!data->GSUsed) ImGui::PopItemFlag();
		}
		else if (m_item.Type == PipelineItem::ItemType::BlendState) {
			pipe::BlendState* data = (pipe::BlendState*)m_item.Data;
			D3D11_BLEND_DESC* desc = &data->State.Info;

			// alpha to coverage
			ImGui::Text("Alpha to coverage:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_alphacov", (bool*)(&desc->AlphaToCoverageEnable));
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// source blend factor
			ImGui::Text("Source blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendCombo("##cui_srcblend", desc->RenderTarget[0].SrcBlend);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// operator
			ImGui::Text("Blend operator:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendOperatorCombo("##cui_blendop", desc->RenderTarget[0].BlendOp);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// destination blend factor
			ImGui::Text("Destination blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendCombo("##cui_destblend", desc->RenderTarget[0].DestBlend);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// source alpha blend factor
			ImGui::Text("Source alpha blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendCombo("##cui_srcalphablend", desc->RenderTarget[0].SrcBlendAlpha);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// operator
			ImGui::Text("Blend operator:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendOperatorCombo("##cui_blendopalpha", desc->RenderTarget[0].BlendOpAlpha);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// destination alpha blend factor
			ImGui::Text("Destination blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendCombo("##cui_destalphablend", desc->RenderTarget[0].DestBlendAlpha);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// blend factor
			ml::Color blendFactor = data->State.GetBlendFactor();
			ImGui::Text("Custom blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateInputColor("##cui_blendfactor", blendFactor);
			ImGui::PopItemWidth();
			ImGui::NextColumn();
			data->State.SetBlendFactor(blendFactor);
		}
		else if (m_item.Type == PipelineItem::ItemType::DepthStencilState) {
			pipe::DepthStencilState* data = (pipe::DepthStencilState*)m_item.Data;
			D3D11_DEPTH_STENCIL_DESC* desc = &data->State.Info;

			// depth enable
			ImGui::Text("Depth test:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_depth", (bool*)(&desc->DepthEnable));
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// depth function
			ImGui::Text("Depth function:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateComparisonFunctionCombo("##cui_depthop", desc->DepthFunc);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// stencil enable
			ImGui::Text("Stencil test:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_stencil", (bool*)(&desc->StencilEnable));
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// front face function
			ImGui::Text("Stencil front face function:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateComparisonFunctionCombo("##cui_ffunc", desc->FrontFace.StencilFunc);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// front face pass operation
			ImGui::Text("Stencil front face pass:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateStencilOperationCombo("##cui_fpass", desc->FrontFace.StencilPassOp);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// front face fail operation
			ImGui::Text("Stencil front face fail:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateStencilOperationCombo("##cui_ffail", desc->FrontFace.StencilFailOp);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// back face function
			ImGui::Text("Stencil back face function:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateComparisonFunctionCombo("##cui_bfunc", desc->BackFace.StencilFunc);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// back face pass operation
			ImGui::Text("Stencil back face pass:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateStencilOperationCombo("##cui_bpass", desc->BackFace.StencilPassOp);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// back face fail operation
			ImGui::Text("Stencil back face fail:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateStencilOperationCombo("##cui_bfail", desc->BackFace.StencilFailOp);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// stencil reference
			ImGui::Text("Stencil reference:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputInt("##cui_sref", (int*)&data->StencilReference); // imgui uint input??
			ImGui::PopItemWidth();
			ImGui::NextColumn();
		}
		else if (m_item.Type == PipelineItem::ItemType::RasterizerState) {
			pipe::RasterizerState* data = (pipe::RasterizerState*)m_item.Data;
			D3D11_RASTERIZER_DESC* desc = &data->State.Info;

			// enable/disable wireframe rendering
			bool isWireframe = desc->FillMode == D3D11_FILL_WIREFRAME;
			ImGui::Text("Wireframe:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_wireframe", (bool*)(&isWireframe));
			ImGui::PopItemWidth();
			ImGui::NextColumn();
			desc->FillMode = isWireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;

			// cull mode
			ImGui::Text("Cull mode:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateCullModeCombo("##cui_cull", desc->CullMode);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// front face == counter clockwise order
			ImGui::Text("Counter clockwise:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_ccw", (bool*)(&desc->FrontCounterClockwise));
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// depth bias
			ImGui::Text("Depth bias:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::DragInt("##cui_depthbias", &desc->DepthBias);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// depth bias clamp
			ImGui::Text("Depth bias clamp:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::DragFloat("##cui_depthbiasclamp", &desc->DepthBiasClamp);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// slope scaled depth bias
			ImGui::Text("Slope scaled depth bias:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::DragFloat("##cui_slopebias", &desc->SlopeScaledDepthBias);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// depth clip
			ImGui::Text("Depth clip:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_depthclip", (bool*)(&desc->DepthClipEnable));
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// antialiasing
			ImGui::Text("Antialiasing:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_aa", (bool*)(&desc->AntialiasedLineEnable));
			ImGui::PopItemWidth();
			ImGui::NextColumn();
		}
		else if (m_item.Type == PipelineItem::ItemType::OBJModel) {
			pipe::OBJModel* data = (pipe::OBJModel*)m_item.Data;

			// model filepath
			ImGui::Text("File:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::InputText("##cui_objfile", data->Filename, MAX_PATH);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_meshfile", ImVec2(-1, 0))) {
				std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_data->GetOwner()->GetWindowHandle(), L"OBJ\0*.obj\0"));
				strcpy(data->Filename, file.c_str());

				if (file.size() > 0) {
					ml::OBJModel mdl;
					mdl.LoadFromFile(file);
					m_groups = mdl.GetObjects();


					std::vector<std::string> grps = mdl.GetGroups();
					for (std::string& str : grps)
						if (std::count(m_groups.begin(), m_groups.end(), str) == 0)
							m_groups.push_back(str);
				}
			}
			ImGui::NextColumn();

			if (m_groups.size() > 0) {
				// should we render only a part of the mesh?
				ImGui::Text("Render group only:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				ImGui::Checkbox("##cui_meshgroup", &data->OnlyGroup);
				ImGui::NextColumn();

				// group name
				if (data->OnlyGroup) {
					ImGui::Text("Group name:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::BeginCombo("##cui_meshgroupname", m_groups[m_selectedGroup].c_str())) {
						for (int i = 0; i < m_groups.size(); i++)
							if (ImGui::Selectable(m_groups[i].c_str(), i == m_selectedGroup)) {
								strcpy(data->GroupName, m_groups[i].c_str());
								m_selectedGroup = i;
							}
						ImGui::EndCombo();
					}

					//ImGui::InputText("##cui_meshgroupname", data->GroupName, MODEL_GROUP_NAME_LENGTH);
					ImGui::NextColumn();
				}
			}
		}

		ImGui::Columns();
	}
	void CreateItemUI::SetOwner(const char * shaderPass)
	{
		if (shaderPass == nullptr)
			m_owner[0] = 0;
		else
			strcpy(m_owner, shaderPass);
	}
	void CreateItemUI::SetType(PipelineItem::ItemType type)
	{
		m_item.Type = type;

		if (m_item.Data != nullptr)
			delete m_item.Data;

		if (m_item.Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem* allocatedData = new pipe::GeometryItem();
			allocatedData->Type = pipe::GeometryItem::GeometryType::Cube;
			allocatedData->Size = allocatedData->Scale = DirectX::XMFLOAT3(1,1,1);
			m_item.Data = allocatedData;
		}
		else if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* allocatedData = new pipe::ShaderPass();
			m_item.Data = allocatedData;
			strcpy(allocatedData->RenderTexture[0], "Window");			
		}
		else if (m_item.Type == PipelineItem::ItemType::BlendState) {
			pipe::BlendState* allocatedData = new pipe::BlendState();
			allocatedData->State.Info.IndependentBlendEnable = false;
			allocatedData->State.Info.RenderTarget[0].BlendEnable = true;
			m_item.Data = allocatedData;
		}
		else if (m_item.Type == PipelineItem::ItemType::DepthStencilState) {
			pipe::DepthStencilState* allocatedData = new pipe::DepthStencilState();
			allocatedData->State.Info.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			m_item.Data = allocatedData;
		}
		else if (m_item.Type == PipelineItem::ItemType::RasterizerState)
			m_item.Data = new pipe::RasterizerState();
		else if (m_item.Type == PipelineItem::ItemType::OBJModel) {
			m_selectedGroup = 0;
			m_groups.clear();

			pipe::OBJModel* allocatedData = new pipe::OBJModel();
			allocatedData->OnlyGroup = false;
			allocatedData->Scale = DirectX::XMFLOAT3(1, 1, 1);
			m_item.Data = allocatedData;
		}
	}
	bool CreateItemUI::Create()
	{
		if (strlen(m_item.Name) < 2)
			return false;

		if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* data = new pipe::ShaderPass();
			pipe::ShaderPass* origData = (pipe::ShaderPass*)m_item.Data;

			strcpy(data->PSEntry, origData->PSEntry);
			strcpy(data->PSPath, origData->PSPath);
			strcpy(data->VSEntry, origData->VSEntry);
			strcpy(data->VSPath, origData->VSPath);
			strcpy(data->RenderTexture[0], origData->RenderTexture[0]);

			return m_data->Pipeline.AddPass(m_item.Name, data);
		} else if (m_owner[0] != 0) {
			if (m_item.Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* data = new pipe::GeometryItem();
				pipe::GeometryItem* origData = (pipe::GeometryItem*)m_item.Data;

				data->Position = DirectX::XMFLOAT3(0, 0, 0);
				data->Rotation = DirectX::XMFLOAT3(0, 0, 0);
				data->Scale = DirectX::XMFLOAT3(1, 1, 1);
				data->Size = origData->Size;
				data->Topology = ml::Topology::TriangleList;
				data->Type = origData->Type;

				if (data->Type == pipe::GeometryItem::GeometryType::Cube)
					data->Geometry = ml::GeometryFactory::CreateCube(data->Size.x, data->Size.y, data->Size.z, *m_data->GetOwner());
				else if (data->Type == pipe::GeometryItem::Circle) {
					data->Geometry = ml::GeometryFactory::CreateCircle(0, 0, data->Size.x, data->Size.y, *m_data->GetOwner());
					data->Topology = ml::Topology::TriangleStrip;
				}
				else if (data->Type == pipe::GeometryItem::Plane)
					data->Geometry = ml::GeometryFactory::CreatePlane(data->Size.x, data->Size.y, *m_data->GetOwner());
				else if (data->Type == pipe::GeometryItem::Rectangle)
					data->Geometry = ml::GeometryFactory::CreatePlane(1, 1, *m_data->GetOwner());
				else if (data->Type == pipe::GeometryItem::Sphere)
					data->Geometry = ml::GeometryFactory::CreateSphere(data->Size.x, *m_data->GetOwner());
				else if (data->Type == pipe::GeometryItem::Triangle)
					data->Geometry = ml::GeometryFactory::CreateTriangle(0, 0, data->Size.x, *m_data->GetOwner());

				return m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
			}
			else if (m_item.Type == PipelineItem::ItemType::BlendState) {
				pipe::BlendState* data = new pipe::BlendState();
				pipe::BlendState* origData = (pipe::BlendState*)m_item.Data;

				data->State.Info = origData->State.Info;
				data->State.SetBlendFactor(origData->State.GetBlendFactor());
				data->State.Create(*m_data->GetOwner());

				return m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
			}
			else if (m_item.Type == PipelineItem::ItemType::DepthStencilState) {
				pipe::DepthStencilState* data = new pipe::DepthStencilState();
				pipe::DepthStencilState* origData = (pipe::DepthStencilState*)m_item.Data;

				data->State.Info = origData->State.Info;
				data->StencilReference = origData->StencilReference;
				data->State.Create(*m_data->GetOwner());

				return m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
			}
			else if (m_item.Type == PipelineItem::ItemType::RasterizerState) {
				pipe::RasterizerState* data = new pipe::RasterizerState();
				pipe::RasterizerState* origData = (pipe::RasterizerState*)m_item.Data;

				data->State.Info = origData->State.Info;
				data->State.Create(*m_data->GetOwner());

				return m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
			}
			else if (m_item.Type == PipelineItem::ItemType::OBJModel) {
				pipe::OBJModel* data = new pipe::OBJModel();
				pipe::OBJModel* origData = (pipe::OBJModel*)m_item.Data;

				strcpy(data->Filename, origData->Filename);
				strcpy(data->GroupName, origData->GroupName);
				data->OnlyGroup = origData->OnlyGroup;
				data->Scale = origData->Scale;
				data->Position = origData->Position;
				data->Rotation = origData->Rotation;
			

				if (strlen(data->Filename) > 0) {
					std::string objMem = m_data->Parser.LoadProjectFile(data->Filename);
					ml::OBJModel* mdl = m_data->Parser.LoadModel(data->Filename);

					bool loaded = mdl != nullptr;
					if (loaded)
						data->Mesh = *mdl;
					else m_data->Messages.Add(ed::MessageStack::Type::Error, m_owner, "Failed to create .obj model " + std::string(m_item.Name));

					// TODO: if (!loaded) error "Failed to load a mesh"

					if (loaded) {
						ml::OBJModel::Vertex* verts = data->Mesh.GetVertexData();
						ml::UInt32 vertCount = data->Mesh.GetVertexCount();

						if (data->OnlyGroup) {
							verts = data->Mesh.GetGroupVertices(data->GroupName);
							vertCount = data->Mesh.GetGroupVertexCount(data->GroupName);

							if (verts == nullptr) {
								verts = data->Mesh.GetObjectVertices(data->GroupName);
								vertCount = data->Mesh.GetObjectVertexCount(data->GroupName);

								// TODO: if (verts == nullptr) error "failed to find a group with that name"
								if (verts == nullptr) return false;
							}
						}

						data->VertCount = vertCount;
						data->Vertices.Create(*m_data->GetOwner(), verts, vertCount, ml::Resource::Immutable);
					}
					else return false;
				}

				return m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
			}
		}

		return false;
	}
}