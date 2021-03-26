#include <SHADERed/UI/CreateItemUI.h>
#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/Engine/Model.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/UIHelper.h>

#include <misc/ImFileDialog.h>

#include <filesystem>
#include <fstream>
#include <string.h>
#include <glm/gtc/type_ptr.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#define HARRAYSIZE(a) (sizeof(a) / sizeof(*a))
#define PATH_SPACE_LEFT Settings::Instance().CalculateSize(-40)

namespace ed {
	CreateItemUI::CreateItemUI(GUIManager* ui, InterfaceManager* objects, const std::string& name, bool visible)
			: UIView(ui, objects, name, visible)
	{
		SetOwner(nullptr);
		m_item.Data = nullptr;
		m_selectedGroup = 0;
		m_errorOccured = false;
		memset(m_owner, 0, PIPELINE_ITEM_NAME_LENGTH * sizeof(char));

		m_dialogPath = nullptr;
		m_dialogShaderAuto = nullptr;
		m_dialogShaderType = "";

		m_fileAutoExtensionSel = 0;
		
		Reset();
	}

	void CreateItemUI::OnEvent(const SDL_Event& e)
	{
	}
	void CreateItemUI::Update(float delta)
	{
		int colWidth = 165;
		if (m_item.Type == PipelineItem::ItemType::RenderState)
			colWidth = 200;

		ImGui::Columns(2, 0, true);

		// TODO: this is only a temprorary fix for non-resizable columns
		static bool isColumnWidthSet = false;
		if (!isColumnWidthSet) {
			ImGui::SetColumnWidth(0, Settings::Instance().CalculateSize(colWidth));
			isColumnWidthSet = true;
		}

		if (m_errorOccured)
			ImGui::PushStyleColor(ImGuiCol_Text, ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme)[(int)TextEditor::PaletteIndex::ErrorMessage]);
		ImGui::Text("Name:");
		if (m_errorOccured)
			ImGui::PopStyleColor();

		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		if (ImGui::InputText("##cui_name", m_item.Name, PIPELINE_ITEM_NAME_LENGTH)) {
			// generate filenames
			m_updateItemFilenames();
		}
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
			} else if (data->Type == pipe::GeometryItem::GeometryType::Circle) {
				ImGui::Text("Radius:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat2("##cui_geosize", glm::value_ptr(data->Size));
				ImGui::NextColumn();
			} else if (data->Type == pipe::GeometryItem::GeometryType::Plane) {
				ImGui::Text("Size:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat2("##cui_geosize", glm::value_ptr(data->Size));
				ImGui::NextColumn();
			} else if (data->Type == pipe::GeometryItem::GeometryType::Sphere) {
				ImGui::Text("Radius:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat("##cui_geosize", &data->Size.x);
				ImGui::NextColumn();
			} else if (data->Type == pipe::GeometryItem::GeometryType::Triangle) {
				ImGui::Text("Size:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat("##cui_geosize", &data->Size.x);
				ImGui::NextColumn();
			}
		}
		else if (m_item.Type == PipelineItem::ItemType::VertexBuffer) {
			pipe::VertexBuffer* data = (pipe::VertexBuffer*)m_item.Data;

		}
		else if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* data = (pipe::ShaderPass*)m_item.Data;

			// file extension / language:
			ImGui::Text("Language:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			if (ImGui::Combo("##cui_splangext", &m_fileAutoExtensionSel, m_fileAutoLanguages.c_str()))
				m_updateItemFilenames();
			ImGui::NextColumn();

			// vs path
			ImGui::Text("Vertex shader path:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::InputText("##cui_spvspath", data->VSPath, SHADERED_MAX_PATH);
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_spvspath", ImVec2(-1, 0))) {
				m_dialogPath = data->VSPath;
				m_dialogShaderAuto = &m_isShaderFileAuto[0];
				m_dialogShaderType = "Vertex";
				ifd::FileDialog::Instance().Open("CreateItemShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.slang,.shader},.*");
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
			ImGui::InputText("##cui_sppspath", data->PSPath, SHADERED_MAX_PATH);
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_sppspath", ImVec2(-1, 0))) {
				m_dialogPath = data->PSPath;
				m_dialogShaderAuto = &m_isShaderFileAuto[1];
				m_dialogShaderType = "Pixel";
				ifd::FileDialog::Instance().Open("CreateItemShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.slang,.shader},.*");
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
			if (ImGui::Checkbox("##cui_spgsuse", &data->GSUsed)) {
				if (m_isShaderFileAuto[2]) {
					if (data->GSUsed)
						strcpy(data->GSPath, ("shaders/" + std::string(m_item.Name) + "GS." + m_fileAutoExtensions[m_fileAutoExtensionSel]).c_str());
					else
						strcpy(data->GSPath, "");
				}
			}
			ImGui::NextColumn();

			if (!data->GSUsed) ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

			// gs path
			ImGui::Text("Geometry shader path:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::InputText("##cui_spgspath", data->GSPath, SHADERED_MAX_PATH);
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_spgspath", ImVec2(-1, 0))) {
				m_dialogPath = data->GSPath;
				m_dialogShaderAuto = &m_isShaderFileAuto[2];
				m_dialogShaderType = "Geometry";
				ifd::FileDialog::Instance().Open("CreateItemShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.slang,.shader},.*");
			}
			ImGui::NextColumn();

			// gs entry
			ImGui::Text("Geometry shader entry:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputText("##cui_spgsentry", data->GSEntry, 32);
			ImGui::NextColumn();

			if (!data->GSUsed) ImGui::PopItemFlag();


			
			// ts used
			ImGui::Text("Use tessellation shader:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			if (ImGui::Checkbox("##cui_sptsuse", &data->TSUsed)) {
				if (m_isShaderFileAuto[3]) {
					if (data->TSUsed)
						strcpy(data->TCSPath, ("shaders/" + std::string(m_item.Name) + "TCS." + m_fileAutoExtensions[m_fileAutoExtensionSel]).c_str());
					else
						strcpy(data->TCSPath, "");
				}
				if (m_isShaderFileAuto[4]) {
					if (data->TSUsed)
						strcpy(data->TESPath, ("shaders/" + std::string(m_item.Name) + "TES." + m_fileAutoExtensions[m_fileAutoExtensionSel]).c_str());
					else
						strcpy(data->TESPath, "");
				}
			}
			ImGui::NextColumn();

			if (!data->TSUsed) ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

			ImGui::Text("Patch vertices:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			if (ImGui::DragInt("##cui_sptspatchverts", &data->TSPatchVertices, 1.0f, 1, m_data->Renderer.GetMaxPatchVertices()))
				data->TSPatchVertices = std::max<int>(1, std::min<int>(m_data->Renderer.GetMaxPatchVertices(), data->TSPatchVertices));
			ImGui::NextColumn();
			ImGui::Separator();

			// tcs path
			ImGui::Text("Tessellation control shader path:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::InputText("##cui_sptcspath", data->TCSPath, SHADERED_MAX_PATH);
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_sptcspath", ImVec2(-1, 0))) {
				m_dialogPath = data->TCSPath;
				m_dialogShaderAuto = &m_isShaderFileAuto[3];
				m_dialogShaderType = "Tessellation Control";
				ifd::FileDialog::Instance().Open("CreateItemShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.tess,.tsc,.tes.slang,.shader},.*");
			}
			ImGui::NextColumn();

			// tcs entry
			ImGui::Text("Tessellation control shader entry:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputText("##cui_sptcsentry", data->TCSEntry, 32);
			ImGui::NextColumn();

			// tes path
			ImGui::Text("Tessellation evaluation shader path:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::InputText("##cui_sptespath", data->TESPath, SHADERED_MAX_PATH);
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_sptespath", ImVec2(-1, 0))) {
				m_dialogPath = data->TESPath;
				m_dialogShaderAuto = &m_isShaderFileAuto[4];
				m_dialogShaderType = "Tessellation Evaluation";
				ifd::FileDialog::Instance().Open("CreateItemShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.tess,.tsc,.tes.slang,.shader},.*");
			}
			ImGui::NextColumn();

			// tes entry
			ImGui::Text("Tessellation evaluation shader entry:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputText("##cui_sptesentry", data->TESEntry, 32);
			ImGui::NextColumn();

			if (!data->TSUsed) ImGui::PopItemFlag();
		}
		else if (m_item.Type == PipelineItem::ItemType::ComputePass) {
			pipe::ComputePass* data = (pipe::ComputePass*)m_item.Data;

			// cs path
			ImGui::Text("Shader path:");
			ImGui::NextColumn();
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::InputText("##cui_cppath", data->Path, SHADERED_MAX_PATH);
			ImGui::PopItemWidth();
			ImGui::PopItemFlag();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_cppath", ImVec2(-1, 0))) {
				m_dialogPath = data->Path;
				m_dialogShaderAuto = &m_isShaderFileAuto[0];
				m_dialogShaderType = "Compute";
				ifd::FileDialog::Instance().Open("CreateItemShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.slang,.shader},.*");
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
		else if (m_item.Type == PipelineItem::ItemType::AudioPass) {
			pipe::AudioPass* data = (pipe::AudioPass*)m_item.Data;

			// ss path
			ImGui::Text("Shader path:");
			ImGui::NextColumn();
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushItemWidth(PATH_SPACE_LEFT);
			ImGui::InputText("##cui_appath", data->Path, SHADERED_MAX_PATH);
			ImGui::PopItemWidth();
			ImGui::PopItemFlag();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_appath", ImVec2(-1, 0))) {
				m_dialogPath = data->Path;
				m_dialogShaderAuto = &m_isShaderFileAuto[0];
				m_dialogShaderType = "Audio";
				ifd::FileDialog::Instance().Open("CreateItemShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.slang,.shader},.*");
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

			if (!data->DepthTest) {
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

			if (!data->DepthTest) {
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

			if (!data->Blend) {
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

			if (!data->Blend) {
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

			if (!data->StencilTest) {
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}

			// stencil mask
			ImGui::Text("Stencil mask:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputScalar("##cui_stencilmask", ImGuiDataType_U8, (int*)&data->StencilMask);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// stencil reference
			ImGui::Text("Stencil reference:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputScalar("##cui_sref", ImGuiDataType_U8, (int*)&data->StencilReference);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

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

			if (!data->StencilTest) {
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
			ImGui::InputText("##cui_objfile", data->Filename, SHADERED_MAX_PATH);
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_meshfile", ImVec2(-1, 0))) {
				m_dialogPath = data->Filename;
				ifd::FileDialog::Instance().Open("CreateMeshDlg", "3D model", ".*");
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


		
		// file dialogs
		if (ifd::FileDialog::Instance().IsDone("CreateItemShaderDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) {
				std::string file = ifd::FileDialog::Instance().GetResult().u8string();
				file = m_data->Parser.GetRelativePath(file);

				if (m_dialogPath != nullptr)
					strcpy(m_dialogPath, file.c_str());

				if (m_dialogShaderAuto != nullptr)
					*m_dialogShaderAuto = false;

				if (m_data->Parser.FileExists(file))
					m_data->Messages.ClearGroup(m_item.Name);
				else
					m_data->Messages.Add(ed::MessageStack::Type::Error, m_item.Name, m_dialogShaderType + " shader file doesnt exist");
			}
			ifd::FileDialog::Instance().Close();
		}
		if (ifd::FileDialog::Instance().IsDone("CreateMeshDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) {
				std::string file = ifd::FileDialog::Instance().GetResult().u8string();
				file = m_data->Parser.GetRelativePath(file);

				strcpy(m_dialogPath, file.c_str());

				eng::Model* mdl = m_data->Parser.LoadModel(m_dialogPath);
				if (mdl != nullptr)
					m_groups = mdl->GetMeshNames();
			}
			ifd::FileDialog::Instance().Close();
		}
	}
	void CreateItemUI::SetOwner(const char* shaderPass)
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
			allocatedData->Size = allocatedData->Scale = glm::vec3(1, 1, 1);
			m_item.Data = allocatedData;
		} else if (m_item.Type == PipelineItem::ItemType::VertexBuffer) {
			Logger::Get().Log("Opening a CreateItemUI for creating VertexBuffer object...");

			pipe::VertexBuffer* allocatedData = new pipe::VertexBuffer();
			allocatedData->Scale = glm::vec3(1, 1, 1);
			allocatedData->Buffer = 0;
			m_item.Data = allocatedData;
		} else if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			Logger::Get().Log("Opening a CreateItemUI for creating ShaderPass object...");

			pipe::ShaderPass* allocatedData = new pipe::ShaderPass();
			strcpy(allocatedData->VSEntry, "main");
			strcpy(allocatedData->PSEntry, "main");
			strcpy(allocatedData->GSEntry, "main");
			m_item.Data = allocatedData;
			m_item.Name[0] = 0;

			allocatedData->RenderTextures[0] = m_data->Renderer.GetTexture();
		} else if (m_item.Type == PipelineItem::ItemType::ComputePass) {
			Logger::Get().Log("Opening a CreateItemUI for creating ComputePass object...");

			pipe::ComputePass* allocatedData = new pipe::ComputePass();
			strcpy(allocatedData->Entry, "main");
			m_item.Data = allocatedData;
			m_item.Name[0] = 0;
		} else if (m_item.Type == PipelineItem::ItemType::AudioPass) {
			Logger::Get().Log("Opening a CreateItemUI for creating AudioPass object...");

			pipe::AudioPass* allocatedData = new pipe::AudioPass();
			m_item.Data = allocatedData;
			m_item.Name[0] = 0;
		} else if (m_item.Type == PipelineItem::ItemType::RenderState) {
			Logger::Get().Log("Opening a CreateItemUI for creating RenderState object...");

			pipe::RenderState* allocatedData = new pipe::RenderState();
			m_item.Data = allocatedData;
		} else if (m_item.Type == PipelineItem::ItemType::Model) {
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
			m_errorOccured = true;
			return false;
		}

		Logger::Get().Log("Executing CreateItemUI::Create()");

		if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* data = new pipe::ShaderPass();
			pipe::ShaderPass* origData = (pipe::ShaderPass*)m_item.Data;

			m_createFile(origData->VSPath);
			m_createFile(origData->PSPath);
			if (origData->GSUsed) {
				m_createFile(origData->GSPath);
				strcpy(data->GSEntry, origData->GSEntry);
				strcpy(data->GSPath, origData->GSPath);
			}
			if (origData->TSUsed) {
				// control shader
				m_createFile(origData->TCSPath);
				strcpy(data->TCSEntry, origData->TCSEntry);
				strcpy(data->TCSPath, origData->TCSPath);

				// evaluation shader
				m_createFile(origData->TESPath);
				strcpy(data->TESEntry, origData->TESEntry);
				strcpy(data->TESPath, origData->TESPath);
			}

			strcpy(data->PSEntry, origData->PSEntry);
			strcpy(data->PSPath, origData->PSPath);
			strcpy(data->VSEntry, origData->VSEntry);
			strcpy(data->VSPath, origData->VSPath);
			data->GSUsed = origData->GSUsed;
			data->TSUsed = origData->TSUsed;
			data->TSPatchVertices = origData->TSPatchVertices;
			data->RenderTextures[0] = origData->RenderTextures[0];
			data->RTCount = 1;
			data->InputLayout = gl::CreateDefaultInputLayout();

			m_errorOccured = !m_data->Pipeline.AddShaderPass(m_item.Name, data);

			if (!m_errorOccured) {
				PipelineItem* actualItem = m_data->Pipeline.Get(m_item.Name);

				CodeEditorUI* code = ((CodeEditorUI*)m_ui->Get(ViewID::Code));
				code->Open(actualItem, ed::ShaderStage::Vertex);
				code->Open(actualItem, ed::ShaderStage::Pixel);

				if (data->GSUsed && data->GSPath[0] != 0)
					code->Open(actualItem, ed::ShaderStage::Geometry);

				if (data->TSUsed) {
					if (data->TCSPath[0] != 0)
						code->Open(actualItem, ed::ShaderStage::TessellationControl);
					if (data->TESPath[0] != 0)
						code->Open(actualItem, ed::ShaderStage::TessellationEvaluation);
				}
			}

			return !m_errorOccured;
		} else if (m_item.Type == PipelineItem::ItemType::ComputePass) {
			pipe::ComputePass* data = new pipe::ComputePass();
			pipe::ComputePass* origData = (pipe::ComputePass*)m_item.Data;

			m_createFile(origData->Path);

			strcpy(data->Entry, origData->Entry);
			strcpy(data->Path, origData->Path);
			data->WorkX = origData->WorkX;
			data->WorkY = origData->WorkY;
			data->WorkZ = origData->WorkZ;

			m_errorOccured = !m_data->Pipeline.AddComputePass(m_item.Name, data);
			return !m_errorOccured;
		} else if (m_item.Type == PipelineItem::ItemType::AudioPass) {
			pipe::AudioPass* data = new pipe::AudioPass();
			pipe::AudioPass* origData = (pipe::AudioPass*)m_item.Data;

			m_createFile(origData->Path);

			strcpy(data->Path, origData->Path);

			m_errorOccured = !m_data->Pipeline.AddAudioPass(m_item.Name, data);
			return !m_errorOccured;
		} else if (m_owner[0] != 0) {
			if (m_item.Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* data = new pipe::GeometryItem();
				pipe::GeometryItem* origData = (pipe::GeometryItem*)m_item.Data;

				std::vector<InputLayoutItem> inpLayout;
				PipelineItem* ownerItem = m_data->Pipeline.Get(m_owner);
				if (ownerItem->Type == PipelineItem::ItemType::ShaderPass)
					inpLayout = ((pipe::ShaderPass*)(ownerItem->Data))->InputLayout;
				else if (ownerItem->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* pdata = ((pipe::PluginItemData*)ownerItem->Data);
					inpLayout = m_data->Plugins.BuildInputLayout(pdata->Owner, pdata->Type, pdata->PluginData);
				}

				data->Position = glm::vec3(0, 0, 0);
				data->Rotation = glm::vec3(0, 0, 0);
				data->Scale = glm::vec3(1, 1, 1);
				data->Size = origData->Size;
				data->Topology = GL_TRIANGLES;
				data->Type = origData->Type;

				if (data->Type == pipe::GeometryItem::GeometryType::Cube)
					data->VAO = eng::GeometryFactory::CreateCube(data->VBO, data->Size.x, data->Size.y, data->Size.z, inpLayout);
				else if (data->Type == pipe::GeometryItem::Circle)
					data->VAO = eng::GeometryFactory::CreateCircle(data->VBO, data->Size.x, data->Size.y, inpLayout);
				else if (data->Type == pipe::GeometryItem::Plane)
					data->VAO = eng::GeometryFactory::CreatePlane(data->VBO, data->Size.x, data->Size.y, inpLayout);
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
			} else if (m_item.Type == PipelineItem::ItemType::RenderState) {
				pipe::RenderState* data = new pipe::RenderState();
				pipe::RenderState* origData = (pipe::RenderState*)m_item.Data;

				memcpy(data, origData, sizeof(pipe::RenderState));

				m_errorOccured = !m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
				return !m_errorOccured;
			} else if (m_item.Type == PipelineItem::ItemType::Model) {
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
					else
						m_data->Messages.Add(ed::MessageStack::Type::Error, m_owner, "Failed to create a 3D model " + std::string(m_item.Name));
				}

				m_errorOccured = !m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
				return !m_errorOccured;
			} else if (m_item.Type == PipelineItem::ItemType::VertexBuffer) {
				pipe::VertexBuffer* data = new pipe::VertexBuffer();
				pipe::VertexBuffer* origData = (pipe::VertexBuffer*)m_item.Data;

				data->Position = glm::vec3(0, 0, 0);
				data->Rotation = glm::vec3(0, 0, 0);
				data->Scale = glm::vec3(1, 1, 1);
				data->Topology = GL_TRIANGLES;
				data->Buffer = origData->Buffer;

				if (data->Buffer != 0)
					gl::CreateBufferVAO(data->VAO, ((ed::BufferObject*)data->Buffer)->ID, m_data->Objects.ParseBufferFormat(((ed::BufferObject*)data->Buffer)->ViewFormat));

				m_errorOccured = !m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
				return !m_errorOccured;
			} 
		}

		return false;
	}
	void CreateItemUI::Reset()
	{
		m_isShaderFileAuto[0] = true;
		m_isShaderFileAuto[1] = true;
		m_isShaderFileAuto[2] = true;
		m_isShaderFileAuto[3] = true;
		m_isShaderFileAuto[4] = true;
	}
	void CreateItemUI::UpdateLanguageList()
	{
		m_fileAutoLanguages = std::string("GLSL\0HLSL\0Vulkan GLSL\0", 22);
		m_fileAutoExtensions = std::vector<std::string>(3);

		m_fileAutoExtensions[0] = "glsl";
		if (Settings::Instance().General.HLSLExtensions.size() > 0)
			m_fileAutoExtensions[1] = Settings::Instance().General.HLSLExtensions[0];
		if (Settings::Instance().General.VulkanGLSLExtensions.size() > 0)
			m_fileAutoExtensions[2] = Settings::Instance().General.VulkanGLSLExtensions[0];

		const auto& plugins = m_data->Plugins.Plugins();
		for (IPlugin1* pl : plugins) {
			int langCount = pl->CustomLanguage_GetCount();
			for (int i = 0; i < langCount; i++) {
				std::string langName = pl->CustomLanguage_GetName(i);
				std::vector<std::string>& extVec = Settings::Instance().General.PluginShaderExtensions[langName];

				m_fileAutoLanguages += langName;
				m_fileAutoLanguages.resize(m_fileAutoLanguages.size() + 1);
				m_fileAutoLanguages[m_fileAutoLanguages.size() - 1] = 0;

				if (extVec.size() > 0)
					m_fileAutoExtensions.push_back(extVec[0]);
				else
					m_fileAutoExtensions.push_back("null");
			}
		}
	}
	void CreateItemUI::m_createFile(const std::string& filename)
	{
		std::string fname = m_data->Parser.GetProjectPath(filename);

		if (!std::filesystem::exists(fname)) {
			std::filesystem::path path(fname);
			if (path.has_parent_path())
				std::filesystem::create_directories(path.parent_path());
			std::ofstream shdr(fname);
			shdr << "// empty shader file\n";
			shdr.close();
		}
	}
	void CreateItemUI::m_updateItemFilenames()
	{
		if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* data = (pipe::ShaderPass*)m_item.Data;

			if (m_isShaderFileAuto[0])
				strcpy(data->VSPath, ("shaders/" + std::string(m_item.Name) + "VS." + m_fileAutoExtensions[m_fileAutoExtensionSel]).c_str());
			if (m_isShaderFileAuto[1])
				strcpy(data->PSPath, ("shaders/" + std::string(m_item.Name) + "PS." + m_fileAutoExtensions[m_fileAutoExtensionSel]).c_str());
			if (m_isShaderFileAuto[2] && data->GSUsed)
				strcpy(data->GSPath, ("shaders/" + std::string(m_item.Name) + "GS." + m_fileAutoExtensions[m_fileAutoExtensionSel]).c_str());
			if (m_isShaderFileAuto[3] && data->TSUsed)
				strcpy(data->TCSPath, ("shaders/" + std::string(m_item.Name) + "TCS." + m_fileAutoExtensions[m_fileAutoExtensionSel]).c_str());
			if (m_isShaderFileAuto[4] && data->TSUsed)
				strcpy(data->TESPath, ("shaders/" + std::string(m_item.Name) + "TES." + m_fileAutoExtensions[m_fileAutoExtensionSel]).c_str());
		} else if (m_item.Type == PipelineItem::ItemType::ComputePass) {
			pipe::ComputePass* data = (pipe::ComputePass*)m_item.Data;

			if (m_isShaderFileAuto[0])
				strcpy(data->Path, ("shaders/" + std::string(m_item.Name) + "CS." + m_fileAutoExtensions[m_fileAutoExtensionSel]).c_str());
		} else if (m_item.Type == PipelineItem::ItemType::AudioPass) {
			pipe::AudioPass* data = (pipe::AudioPass*)m_item.Data;

			if (m_isShaderFileAuto[0])
				strcpy(data->Path, ("shaders/" + std::string(m_item.Name) + "AS.glsl").c_str());
		}
	}
}
