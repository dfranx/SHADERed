#include "CreateItemUI.h"
#include "UIHelper.h"
#include "../Objects/Logger.h"
#include "../Objects/Names.h"
#include "../Objects/Settings.h"
#include "../Objects/ThemeContainer.h"
#include "../Objects/ShaderTranscompiler.h"
#include "../Objects/SystemVariableManager.h"
#include "../Engine/GeometryFactory.h"
#include "../Engine/Model.h"
#include "../Engine/GLUtils.h"

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

		ImGui::Columns(2, 0, true);
		
		// TODO: this is only a temprorary fix for non-resizable columns
		static bool isColumnWidthSet = false;
		if (!isColumnWidthSet) {
			ImGui::SetColumnWidth(0, colWidth);
			isColumnWidthSet= true;
		}

		if (m_errorOccured)
			ImGui::PushStyleColor(ImGuiCol_Text, ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme)[(int)TextEditor::PaletteIndex::ErrorMessage]);
		ImGui::Text("Name:");
		if (m_errorOccured)
			ImGui::PopStyleColor();

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
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::InputText("##cui_spvspath", data->VSPath, MAX_PATH);
			ImGui::PopItemFlag();
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
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::InputText("##cui_sppspath", data->PSPath, MAX_PATH);
			ImGui::PopItemFlag();
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
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::InputText("##cui_spgspath", data->GSPath, MAX_PATH);
			ImGui::PopItemFlag();
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
		else if (m_item.Type == PipelineItem::ItemType::ComputePass)
		{
			pipe::ComputePass *data = (pipe::ComputePass *)m_item.Data;

			// cs path
			ImGui::Text("Shader path:");
			ImGui::NextColumn();
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::InputText("##cui_cppath", data->Path, MAX_PATH);
			ImGui::PopItemWidth();
			ImGui::PopItemFlag();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_cppath", ImVec2(-1, 0)))
			{
				std::string file;
				bool success = UIHelper::GetOpenFileDialog(file);
				if (success)
				{
					file = m_data->Parser.GetRelativePath(file);
					strcpy(data->Path, file.c_str());

					if (m_data->Parser.FileExists(file))
						m_data->Messages.ClearGroup(m_item.Name);
					else
						m_data->Messages.Add(ed::MessageStack::Type::Error, m_item.Name, "Compute shader file doesnt exist");
				}
			}
			ImGui::NextColumn();

			// entry
			ImGui::Text("Shader entry:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputText("##cui_cpentry", data->Entry, 32);
			ImGui::NextColumn();

			// group size
			glm::ivec3 groupSize(data->WorkX, data->WorkY, data->WorkZ);
			ImGui::Text("Group size:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputInt3("##cui_cpgsize", glm::value_ptr(groupSize));
			ImGui::NextColumn();

			data->WorkX = std::max<int>(groupSize.x, 1);
			data->WorkY = std::max<int>(groupSize.y, 1);
			data->WorkZ = std::max<int>(groupSize.z, 1);
		}
		else if (m_item.Type == PipelineItem::ItemType::AudioPass)
		{
			pipe::AudioPass *data = (pipe::AudioPass *)m_item.Data;

			// ss path
			ImGui::Text("Shader path:");
			ImGui::NextColumn();
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::InputText("##cui_appath", data->Path, MAX_PATH);
			ImGui::PopItemWidth();
			ImGui::PopItemFlag();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_appath", ImVec2(-1, 0)))
			{
				std::string file;
				bool success = UIHelper::GetOpenFileDialog(file);
				if (success)
				{
					file = m_data->Parser.GetRelativePath(file);
					strcpy(data->Path, file.c_str());

					if (m_data->Parser.FileExists(file))
						m_data->Messages.ClearGroup(m_item.Name);
					else
						m_data->Messages.Add(ed::MessageStack::Type::Error, m_item.Name, "Audio shader file doesnt exist");
				}
			}
			ImGui::NextColumn();
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
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::InputText("##cui_objfile", data->Filename, MAX_PATH);
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_meshfile", ImVec2(-1, 0))) {
				std::string file;
				bool success = UIHelper::GetOpenFileDialog(file);
				if (success) {
					file = m_data->Parser.GetRelativePath(file);
					strcpy(data->Filename, file.c_str());

					eng::Model* mdl = m_data->Parser.LoadModel(data->Filename);

					if (mdl != nullptr)
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
		m_errorOccured = false;
		if (shaderPass == nullptr)
			m_owner[0] = 0;
		else
			strcpy(m_owner, shaderPass);
	}
	void CreateItemUI::SetType(PipelineItem::ItemType type)
	{
		m_errorOccured = false;
		
		//TODO: make it type-safe.
		m_data->Pipeline.FreeData(m_item.Data, m_item.Type);
		
		m_item.Type = type;

		if (m_item.Type == PipelineItem::ItemType::Geometry) {
			Logger::Get().Log("Opening a CreateItemUI for creating Geometry object...");

			pipe::GeometryItem* allocatedData = new pipe::GeometryItem();
			allocatedData->Type = pipe::GeometryItem::GeometryType::Cube;
			allocatedData->Size = allocatedData->Scale = glm::vec3(1,1,1);
			m_item.Data = allocatedData;
		}
		else if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			Logger::Get().Log("Opening a CreateItemUI for creating ShaderPass object...");

			pipe::ShaderPass *allocatedData = new pipe::ShaderPass();
			strcpy(allocatedData->VSEntry, "main");
			strcpy(allocatedData->PSEntry, "main");
			strcpy(allocatedData->GSEntry, "main");
			m_item.Data = allocatedData;

			allocatedData->RenderTextures[0] = m_data->Renderer.GetTexture();
		} else if (m_item.Type == PipelineItem::ItemType::ComputePass) {
			Logger::Get().Log("Opening a CreateItemUI for creating ComputePass object...");

			pipe::ComputePass *allocatedData = new pipe::ComputePass();
			strcpy(allocatedData->Entry, "main");
			m_item.Data = allocatedData;
		} else if (m_item.Type == PipelineItem::ItemType::AudioPass) {
			Logger::Get().Log("Opening a CreateItemUI for creating AudioPass object...");

			pipe::AudioPass *allocatedData = new pipe::AudioPass();
			m_item.Data = allocatedData;
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
	void CreateItemUI::m_autoVariablePopulate(pipe::ShaderPass* shader)
	{
		if (m_data->Parser.FileExists(shader->VSPath)) {
			std::string psContent = "", vsContent = "",
				vsEntry = shader->VSEntry,
				psEntry = shader->PSEntry;

			// pixel shader
			if (ShaderTranscompiler::GetShaderTypeFromExtension(shader->PSPath) == ShaderLanguage::GLSL)
				psContent = m_data->Parser.LoadProjectFile(shader->PSPath);
			else {
				psContent = ShaderTranscompiler::Transcompile(ShaderTranscompiler::GetShaderTypeFromExtension(shader->PSPath), m_data->Parser.GetProjectPath(std::string(shader->PSPath)), 1, shader->PSEntry, shader->Macros, shader->GSUsed, nullptr, &m_data->Parser);
				psEntry = "main";
			}
			GLuint ps = gl::CompileShader(GL_FRAGMENT_SHADER, psContent.c_str());

			// vertex shader
			if (ShaderTranscompiler::GetShaderTypeFromExtension(shader->VSPath) == ShaderLanguage::GLSL)
				vsContent = m_data->Parser.LoadProjectFile(shader->VSPath);
			else {
				vsContent = ShaderTranscompiler::Transcompile(ShaderTranscompiler::GetShaderTypeFromExtension(shader->VSPath), m_data->Parser.GetProjectPath(std::string(shader->VSPath)), 0, shader->VSEntry, shader->Macros, shader->GSUsed, nullptr, &m_data->Parser);
				vsEntry = "main";
			}
			GLuint vs = gl::CompileShader(GL_VERTEX_SHADER, vsContent.c_str());

			// geometry shader
			GLuint gs = 0;
			if (shader->GSUsed && strlen(shader->GSPath) > 0 && strlen(shader->GSEntry) > 0) {
				std::string gsContent = "",
					gsEntry = shader->GSEntry;

				if (ShaderTranscompiler::GetShaderTypeFromExtension(shader->GSPath) == ShaderLanguage::GLSL)
					gsContent = m_data->Parser.LoadProjectFile(shader->GSPath);
				else { // HLSL / VK
					gsContent = ShaderTranscompiler::Transcompile(ShaderTranscompiler::GetShaderTypeFromExtension(shader->GSPath), m_data->Parser.GetProjectPath(std::string(shader->GSPath)), 2, shader->GSEntry, shader->Macros, shader->GSUsed, nullptr, &m_data->Parser);
					gsEntry = "main";
				}

				gs = gl::CompileShader(GL_GEOMETRY_SHADER, gsContent.c_str());
			}

			GLuint prog = glCreateProgram();
			glAttachShader(prog, vs);
			glAttachShader(prog, ps);
			if (shader->GSUsed) glAttachShader(prog, gs);
			glLinkProgram(prog);

			if (prog != 0) {
				GLint count;

				const GLsizei bufSize = 64; // maximum name length
				GLchar name[bufSize]; // variable name in GLSL
				GLsizei length; // name length
				GLuint samplerLoc = 0;

				glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &count);
				for (GLuint i = 0; i < count; i++)
				{
					GLint size;
					GLenum type;

					glGetActiveUniform(prog, (GLuint)i, bufSize, &length, &size, &type, name);

					ShaderVariable::ValueType valType = ShaderVariable::ValueType::Count;

					if (type == GL_FLOAT)
						valType = ShaderVariable::ValueType::Float1;
					else if (type == GL_FLOAT_VEC2)
						valType = ShaderVariable::ValueType::Float2;
					else if (type == GL_FLOAT_VEC3)
						valType = ShaderVariable::ValueType::Float3;
					else if (type == GL_FLOAT_VEC4)
						valType = ShaderVariable::ValueType::Float4;
					else if (type == GL_INT)
						valType = ShaderVariable::ValueType::Integer1;
					else if (type == GL_INT_VEC2)
						valType = ShaderVariable::ValueType::Integer2;
					else if (type == GL_INT_VEC3)
						valType = ShaderVariable::ValueType::Integer3;
					else if (type == GL_INT_VEC4)
						valType = ShaderVariable::ValueType::Integer4;
					else if (type == GL_BOOL)
						valType = ShaderVariable::ValueType::Boolean1;
					else if (type == GL_BOOL_VEC2)
						valType = ShaderVariable::ValueType::Boolean2;
					else if (type == GL_BOOL_VEC3)
						valType = ShaderVariable::ValueType::Boolean3;
					else if (type == GL_BOOL_VEC4)
						valType = ShaderVariable::ValueType::Boolean4;
					else if (type == GL_FLOAT_MAT2)
						valType = ShaderVariable::ValueType::Float2x2;
					else if (type == GL_FLOAT_MAT3)
						valType = ShaderVariable::ValueType::Float3x3;
					else if (type == GL_FLOAT_MAT4)
						valType = ShaderVariable::ValueType::Float4x4;

					if (valType != ShaderVariable::ValueType::Count) {
						ed::SystemShaderVariable sysType = m_autoSystemValue(name);
						if (SystemVariableManager::GetType(sysType) != valType)
							sysType = SystemShaderVariable::None;
						shader->Variables.Add(new ed::ShaderVariable(valType, name, sysType));
					}
				}
			}
		}
	}
	ed::SystemShaderVariable CreateItemUI::m_autoSystemValue(const std::string& name)
	{
		std::string vname = name;
		std::transform(vname.begin(), vname.end(), vname.begin(), tolower);

		// list of rules for detection:
		if (vname.find("time") != std::string::npos && vname.find("d") == std::string::npos && vname.find("del") == std::string::npos && vname.find("delta") == std::string::npos)
			return SystemShaderVariable::Time;
		else if (vname.find("time") != std::string::npos && (vname.find("d") != std::string::npos || vname.find("del") != std::string::npos || vname.find("delta") != std::string::npos))
			return SystemShaderVariable::TimeDelta;
		else if (vname.find("frame") != std::string::npos || vname.find("index") != std::string::npos)
			return SystemShaderVariable::FrameIndex;
		else if (vname.find("size") != std::string::npos || vname.find("window") != std::string::npos || vname.find("viewport") != std::string::npos || vname.find("resolution") != std::string::npos || vname.find("res") != std::string::npos)
			return SystemShaderVariable::ViewportSize;
		else if (vname.find("mouse") != std::string::npos || vname == "mpos")
			return SystemShaderVariable::MousePosition;
		else if (vname == "view" || vname == "matview" || vname == "matv" || vname == "mview")
			return SystemShaderVariable::View;
		else if (vname == "proj" || vname == "matproj" || vname == "matp" || vname == "mproj" || vname == "projection" || vname == "matprojection" || vname == "mprojection")
			return SystemShaderVariable::Projection;
		else if (vname == "matvp" || vname == "matviewproj" || vname == "matviewprojection" || vname == "viewprojection" || vname == "viewproj" || vname == "mvp")
			return SystemShaderVariable::ViewProjection;
		else if (vname.find("ortho") != std::string::npos)
			return SystemShaderVariable::Orthographic;
		else if (vname.find("geo") != std::string::npos || vname.find("model") != std::string::npos)
			return SystemShaderVariable::GeometryTransform;
		else if (vname.find("pick") != std::string::npos)
			return SystemShaderVariable::IsPicked;
		else if (vname.find("cam") != std::string::npos)
			return SystemShaderVariable::CameraPosition3;
		else if (vname.find("keys") != std::string::npos || vname.find("wasd") != std::string::npos)
			return SystemShaderVariable::KeysWASD;

		return SystemShaderVariable::None;
	}
	bool CreateItemUI::Create()
	{
		if (strlen(m_item.Name) < 2) {
			Logger::Get().Log("CreateItemUI item name is less than 2 characters", true);
			m_errorOccured = true;
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
			data->InputLayout = gl::CreateDefaultInputLayout();

			m_autoVariablePopulate(data);

			m_errorOccured = !m_data->Pipeline.AddShaderPass(m_item.Name, data);
			return !m_errorOccured;
		}
		else if (m_item.Type == PipelineItem::ItemType::ComputePass)
		{
			pipe::ComputePass *data = new pipe::ComputePass();
			pipe::ComputePass *origData = (pipe::ComputePass *)m_item.Data;

			strcpy(data->Entry, origData->Entry);
			strcpy(data->Path, origData->Path);
			data->WorkX = origData->WorkX;
			data->WorkY = origData->WorkY;
			data->WorkZ = origData->WorkZ;

			m_errorOccured = !m_data->Pipeline.AddComputePass(m_item.Name, data);
			return !m_errorOccured;
		}
		else if (m_item.Type == PipelineItem::ItemType::AudioPass)
		{
			pipe::AudioPass *data = new pipe::AudioPass();
			pipe::AudioPass *origData = (pipe::AudioPass *)m_item.Data;

			strcpy(data->Path, origData->Path);

			m_errorOccured = !m_data->Pipeline.AddAudioPass(m_item.Name, data);
			return !m_errorOccured;
		}
		else if (m_owner[0] != 0) {
			if (m_item.Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* data = new pipe::GeometryItem();
				pipe::GeometryItem* origData = (pipe::GeometryItem*)m_item.Data;

				std::vector<InputLayoutItem> inpLayout;
				PipelineItem* ownerItem = m_data->Pipeline.Get(m_owner);
				if (ownerItem->Type == PipelineItem::ItemType::ShaderPass)
					inpLayout = ((pipe::ShaderPass*)(ownerItem->Data))->InputLayout;
				else if (ownerItem->Type == PipelineItem::ItemType::PluginItem)
					inpLayout = m_data->Plugins.BuildInputLayout(((pipe::PluginItemData*)ownerItem->Data)->Owner, m_owner);

				data->Position = glm::vec3(0, 0, 0);
				data->Rotation = glm::vec3(0, 0, 0);
				data->Scale = glm::vec3(1, 1, 1);
				data->Size = origData->Size;
				data->Topology = GL_TRIANGLES;
				data->Type = origData->Type;

				if (data->Type == pipe::GeometryItem::GeometryType::Cube)
					data->VAO = eng::GeometryFactory::CreateCube(data->VBO, data->Size.x, data->Size.y, data->Size.z, inpLayout);
				else if (data->Type == pipe::GeometryItem::Circle) {
					data->VAO = eng::GeometryFactory::CreateCircle(data->VBO, data->Size.x, data->Size.y, inpLayout);
					data->Topology = GL_TRIANGLE_STRIP;
				}
				else if (data->Type == pipe::GeometryItem::Plane)
					data->VAO = eng::GeometryFactory::CreatePlane(data->VBO,data->Size.x, data->Size.y, inpLayout);
				else if (data->Type == pipe::GeometryItem::Rectangle)
					data->VAO = eng::GeometryFactory::CreatePlane(data->VBO, 1, 1, inpLayout);
				else if (data->Type == pipe::GeometryItem::Sphere)
					data->VAO = eng::GeometryFactory::CreateSphere(data->VBO, data->Size.x, inpLayout);
				else if (data->Type == pipe::GeometryItem::Triangle)
					data->VAO = eng::GeometryFactory::CreateTriangle(data->VBO, data->Size.x, inpLayout);
				else if (data->Type == pipe::GeometryItem::ScreenQuadNDC)
					data->VAO = eng::GeometryFactory::CreateScreenQuadNDC(data->VBO, inpLayout);

				m_errorOccured = !m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
				return !m_errorOccured;
			}
			else if (m_item.Type == PipelineItem::ItemType::RenderState) {
				pipe::RenderState* data = new pipe::RenderState();
				pipe::RenderState* origData = (pipe::RenderState*)m_item.Data;

				memcpy(data, origData, sizeof(pipe::RenderState));

				m_errorOccured = !m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
				return !m_errorOccured;
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

				m_errorOccured = !m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
				return !m_errorOccured;
			}
		}

		return false;
	}
}
