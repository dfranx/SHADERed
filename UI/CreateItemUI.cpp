#include "CreateItemUI.h"
#include "UIHelper.h"
#include "../Objects/Logger.h"
#include "../Objects/Names.h"
#include "../Objects/Settings.h"
#include "../Engine/GeometryFactory.h"
#include "../Engine/Model.h"

#include <glm/gtc/type_ptr.hpp>
#include <string.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#define HARRAYSIZE(a) (sizeof(a)/sizeof(*a))
#define PATH_SPACE_LEFT -40 * Settings::Instance().DPIScale

namespace ed
{
	void CreateItemUI::OnEvent(const SDL_Event& e)
	{
	}
	void CreateItemUI::Update(float delta)
	{
		int colWidth = 150;
		if (m_item.Type == PipelineItem::ItemType::RenderState)
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
			ImGui::Combo("##cui_geotype", reinterpret_cast<int*>(&data->Type), GEOMETRY_NAMES, HARRAYSIZE(GEOMETRY_NAMES));
			ImGui::NextColumn();

			//size
			if (data->Type == pipe::GeometryItem::GeometryType::Cube) {
				ImGui::Text("Size:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat3("##cui_geosize", glm::value_ptr(data->Size));
				ImGui::NextColumn();
			}
			else if (data->Type == pipe::GeometryItem::GeometryType::Circle) {
				ImGui::Text("Radius:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat2("##cui_geosize", glm::value_ptr(data->Size));
				ImGui::NextColumn();
			}
			else if (data->Type == pipe::GeometryItem::GeometryType::Plane) {
				ImGui::Text("Size:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat2("##cui_geosize", glm::value_ptr(data->Size));
				ImGui::NextColumn();
			}
			else if (data->Type == pipe::GeometryItem::GeometryType::Sphere) {
				ImGui::Text("Radius:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat("##cui_geosize", &data->Size.x);
				ImGui::NextColumn();
			}
			else if (data->Type == pipe::GeometryItem::GeometryType::Triangle) {
				ImGui::Text("Size:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat("##cui_geosize", &data->Size.x);
				ImGui::NextColumn();
			}
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
				std::string file;
				bool success = UIHelper::GetOpenFileDialog(file);
				if (success) {
					file = m_data->Parser.GetRelativePath(file);
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
				std::string file;
				bool success = UIHelper::GetOpenFileDialog(file);
				if (success) {
					file = m_data->Parser.GetRelativePath(file);
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
				std::string file;
				bool success = UIHelper::GetOpenFileDialog(file);
				if (success) {
					file = m_data->Parser.GetRelativePath(file);
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
		else if (m_item.Type == PipelineItem::ItemType::RenderState) {
			pipe::RenderState* data = (pipe::RenderState*)m_item.Data;

			// enable/disable wireframe rendering
			bool isWireframe = data->PolygonMode == GL_LINE;
			ImGui::Text("Wireframe:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_wireframe", (bool*)(&isWireframe));
			ImGui::PopItemWidth();
			ImGui::NextColumn();
			data->PolygonMode = isWireframe ? GL_LINE : GL_FILL;

			// cull
			ImGui::Text("Cull:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_cullm", &data->CullFace);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// cull mode
			ImGui::Text("Cull mode:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateCullModeCombo("##cui_culltype", data->CullFaceType);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// front face == counter clockwise order
			bool isCCW = data->FrontFace == GL_CCW;
			ImGui::Text("Counter clockwise:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_ccw", &isCCW);
			ImGui::PopItemWidth();
			ImGui::NextColumn();
			data->FrontFace = isCCW ? GL_CCW : GL_CW;



			ImGui::Separator();



			// depth enable
			ImGui::Text("Depth test:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_depth", &data->DepthTest);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			if (!data->DepthTest)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}

			// depth clip
			ImGui::Text("Depth clip:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_depthclip", &data->DepthClamp);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// depth mask
			ImGui::Text("Depth mask:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_depthmask", &data->DepthMask);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// depth function
			ImGui::Text("Depth function:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateComparisonFunctionCombo("##cui_depthop", data->DepthFunction);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// depth bias
			ImGui::Text("Depth bias:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::DragFloat("##cui_depthbias", &data->DepthBias);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			if (!data->DepthTest)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}



			ImGui::Separator();



			// blending
			ImGui::Text("Blending:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_blend", &data->Blend);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			if (!data->Blend)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}

			// alpha to coverage
			ImGui::Text("Alpha to coverage:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_alphacov", &data->AlphaToCoverage);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// source blend factor
			ImGui::Text("Source blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendCombo("##cui_srcblend", data->BlendSourceFactorRGB);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// operator
			ImGui::Text("Blend operator:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendOperatorCombo("##cui_blendop", data->BlendFunctionColor);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// destination blend factor
			ImGui::Text("Destination blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendCombo("##cui_destblend", data->BlendDestinationFactorRGB);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// source alpha blend factor
			ImGui::Text("Source alpha blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendCombo("##cui_srcalphablend", data->BlendSourceFactorAlpha);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// operator
			ImGui::Text("Alpha blend operator:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendOperatorCombo("##cui_blendopalpha", data->BlendFunctionAlpha);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// destination alpha blend factor
			ImGui::Text("Destination alpha blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateBlendCombo("##cui_destalphablend", data->BlendDestinationFactorAlpha);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// blend factor
			ImGui::Text("Custom blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::ColorEdit4("##cui_blendfactor", glm::value_ptr(data->BlendFactor));
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			if (!data->Blend)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}


			ImGui::Separator();



			// stencil enable
			ImGui::Text("Stencil test:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_stencil", &data->StencilTest);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			if (!data->StencilTest)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}

			// stencil mask
			ImGui::Text("Stencil mask:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputInt("##cui_stencilmask", (int*)&data->StencilMask);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			if (data->StencilMask < 0)
				data->StencilMask = 0;
			if (data->StencilMask > 255)
				data->StencilMask = 255;

			// stencil reference
			ImGui::Text("Stencil reference:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputInt("##cui_sref", (int*)& data->StencilReference); // TODO: imgui uint input??
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			if (data->StencilReference < 0)
				data->StencilReference = 0;
			if (data->StencilReference > 255)
				data->StencilReference = 255;

			// front face function
			ImGui::Text("Stencil front face function:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateComparisonFunctionCombo("##cui_ffunc", data->StencilFrontFaceFunction);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// front face pass operation
			ImGui::Text("Stencil front face pass:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateStencilOperationCombo("##cui_fpass", data->StencilFrontFaceOpPass);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// front face stencil fail operation
			ImGui::Text("Stencil front face fail:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateStencilOperationCombo("##cui_ffail", data->StencilFrontFaceOpStencilFail);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// front face depth fail operation
			ImGui::Text("Depth front face fail:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateStencilOperationCombo("##cui_fdfail", data->StencilFrontFaceOpDepthFail);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// back face function
			ImGui::Text("Stencil back face function:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateComparisonFunctionCombo("##cui_bfunc", data->StencilBackFaceFunction);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// back face pass operation
			ImGui::Text("Stencil back face pass:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateStencilOperationCombo("##cui_bpass", data->StencilBackFaceOpPass);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// back face stencil fail operation
			ImGui::Text("Stencil back face fail:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateStencilOperationCombo("##cui_bfail", data->StencilBackFaceOpStencilFail);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// back face depth fail operation
			ImGui::Text("Depth back face fail:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateStencilOperationCombo("##cui_bdfail", data->StencilBackFaceOpDepthFail);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			if (!data->StencilTest)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}
		}
		else if (m_item.Type == PipelineItem::ItemType::Model) {
			pipe::Model* data = (pipe::Model*)m_item.Data;

			// model filepath
			ImGui::Text("File:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::InputText("##cui_objfile", data->Filename, MAX_PATH);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_meshfile", ImVec2(-1, 0))) {
				std::string file;
				bool success = UIHelper::GetOpenFileDialog(file);
				if (success) {
					file = m_data->Parser.GetRelativePath(file);
					strcpy(data->Filename, file.c_str());

					eng::Model* mdl = m_data->Parser.LoadModel(data->Filename);
					m_groups = mdl->GetMeshNames();
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
			Logger::Get().Log("Opening a CreateItemUI for creating Geometry object...");

			pipe::GeometryItem* allocatedData = new pipe::GeometryItem();
			allocatedData->Type = pipe::GeometryItem::GeometryType::Cube;
			allocatedData->Size = allocatedData->Scale = glm::vec3(1,1,1);
			m_item.Data = allocatedData;
		}
		else if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			Logger::Get().Log("Opening a CreateItemUI for creating ShaderPass object...");

			pipe::ShaderPass* allocatedData = new pipe::ShaderPass();
			strcpy(allocatedData->VSEntry, "main");
			strcpy(allocatedData->PSEntry, "main");
			strcpy(allocatedData->GSEntry, "main");
			m_item.Data = allocatedData;
			
			allocatedData->RenderTextures[0] = m_data->Renderer.GetTexture();		
		}
		else if (m_item.Type == PipelineItem::ItemType::RenderState) {
			Logger::Get().Log("Opening a CreateItemUI for creating RenderState object...");

			pipe::RenderState* allocatedData = new pipe::RenderState();
			m_item.Data = allocatedData;
		}
		else if (m_item.Type == PipelineItem::ItemType::Model) {
			Logger::Get().Log("Opening a CreateItemUI for creating Model object...");

			m_selectedGroup = 0;
			m_groups.clear();

			pipe::Model* allocatedData = new pipe::Model();
			allocatedData->OnlyGroup = false;
			allocatedData->Scale = glm::vec3(1, 1, 1);
			m_item.Data = allocatedData;
		}
	}
	bool CreateItemUI::Create()
	{
		if (strlen(m_item.Name) < 2) {
			Logger::Get().Log("CreateItemUI item name is less than 2 characters", true);
			return false;
		}

		Logger::Get().Log("Executing CreateItemUI::Create()");

		if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* data = new pipe::ShaderPass();
			pipe::ShaderPass* origData = (pipe::ShaderPass*)m_item.Data;

			strcpy(data->PSEntry, origData->PSEntry);
			strcpy(data->PSPath, origData->PSPath);
			strcpy(data->VSEntry, origData->VSEntry);
			strcpy(data->VSPath, origData->VSPath);
			data->RenderTextures[0] = origData->RenderTextures[0];
			data->RTCount = 1;

			return m_data->Pipeline.AddPass(m_item.Name, data);
		}
		else if (m_owner[0] != 0) {
			if (m_item.Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* data = new pipe::GeometryItem();
				pipe::GeometryItem* origData = (pipe::GeometryItem*)m_item.Data;

				data->Position = glm::vec3(0, 0, 0);
				data->Rotation = glm::vec3(0, 0, 0);
				data->Scale = glm::vec3(1, 1, 1);
				data->Size = origData->Size;
				data->Topology = GL_TRIANGLES;
				data->Type = origData->Type;

				if (data->Type == pipe::GeometryItem::GeometryType::Cube)
					data->VAO = eng::GeometryFactory::CreateCube(data->VBO, data->Size.x, data->Size.y, data->Size.z);
				else if (data->Type == pipe::GeometryItem::Circle) {
					data->VAO = eng::GeometryFactory::CreateCircle(data->VBO, data->Size.x, data->Size.y);
					data->Topology = GL_TRIANGLE_STRIP;
				}
				else if (data->Type == pipe::GeometryItem::Plane)
					data->VAO = eng::GeometryFactory::CreatePlane(data->VBO,data->Size.x, data->Size.y);
				else if (data->Type == pipe::GeometryItem::Rectangle)
					data->VAO = eng::GeometryFactory::CreatePlane(data->VBO, 1, 1);
				else if (data->Type == pipe::GeometryItem::Sphere)
					data->VAO = eng::GeometryFactory::CreateSphere(data->VBO, data->Size.x);
				else if (data->Type == pipe::GeometryItem::Triangle)
					data->VAO = eng::GeometryFactory::CreateTriangle(data->VBO, data->Size.x);
				else if (data->Type == pipe::GeometryItem::ScreenQuadNDC)
					data->VAO = eng::GeometryFactory::CreateScreenQuadNDC(data->VBO);

				return m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
			}
			else if (m_item.Type == PipelineItem::ItemType::RenderState) {
				pipe::RenderState* data = new pipe::RenderState();
				pipe::RenderState* origData = (pipe::RenderState*)m_item.Data;

				memcpy(data, origData, sizeof(pipe::RenderState));

				return m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
			}
			else if (m_item.Type == PipelineItem::ItemType::Model) {
				pipe::Model* data = new pipe::Model();
				pipe::Model* origData = (pipe::Model*)m_item.Data;

				strcpy(data->Filename, origData->Filename);
				strcpy(data->GroupName, origData->GroupName);
				data->OnlyGroup = origData->OnlyGroup;
				data->Scale = origData->Scale;
				data->Position = origData->Position;
				data->Rotation = origData->Rotation;
			

				if (strlen(data->Filename) > 0) {
					eng::Model* mdl = m_data->Parser.LoadModel(data->Filename);

					bool loaded = mdl != nullptr;
					if (loaded)
						data->Data = mdl;
					else m_data->Messages.Add(ed::MessageStack::Type::Error, m_owner, "Failed to create a 3D model " + std::string(m_item.Name));
				}

				return m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
			}
		}

		return false;
	}
}