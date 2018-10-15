#include "PipelineUI.h"
#include "PropertyUI.h"
#include "PinnedUI.h"
#include "../Options.h"
#include "../GUIManager.h"
#include "../ImGUI/imgui.h"
#include "../Objects/SystemVariableManager.h"

#include <algorithm>

/* todo: move these constants to a different file */
static const char* FORMAT_NAMES[] = {
	"UNKNOWN\0",
	"R32G32B32A32_TYPELESS\0",
	"R32G32B32A32_FLOAT\0",
	"R32G32B32A32_UINT\0",
	"R32G32B32A32_SINT\0",
	"R32G32B32_TYPELESS\0",
	"R32G32B32_FLOAT\0",
	"R32G32B32_UINT\0",
	"R32G32B32_SINT\0",
	"R16G16B16A16_TYPELESS\0",
	"R16G16B16A16_FLOAT\0",
	"R16G16B16A16_UNORM\0",
	"R16G16B16A16_UINT\0",
	"R16G16B16A16_SNORM\0",
	"R16G16B16A16_SINT\0",
	"R32G32_TYPELESS\0",
	"R32G32_FLOAT\0",
	"R32G32_UINT\0",
	"R32G32_SINT\0",
	"R32G8X24_TYPELESS\0",
	"D32_FLOAT_S8X24_UINT\0",
	"R32_FLOAT_X8X24_TYPELESS\0",
	"X32_TYPELESS_G8X24_UINT\0",
	"R10G10B10A2_TYPELESS\0",
	"R10G10B10A2_UNORM\0",
	"R10G10B10A2_UINT\0",
	"R11G11B10_FLOAT\0",
	"R8G8B8A8_TYPELESS\0",
	"R8G8B8A8_UNORM\0",
	"R8G8B8A8_UNORM_SRGB\0",
	"R8G8B8A8_UINT\0",
	"R8G8B8A8_SNORM\0",
	"R8G8B8A8_SINT\0",
	"R16G16_TYPELESS\0",
	"R16G16_FLOAT\0",
	"R16G16_UNORM\0",
	"R16G16_UINT\0",
	"R16G16_SNORM\0",
	"R16G16_SINT\0",
	"R32_TYPELESS\0",
	"D32_FLOAT\0",
	"R32_FLOAT\0",
	"R32_UINT\0",
	"R32_SINT\0",
	"R24G8_TYPELESS\0",
	"D24_UNORM_S8_UINT\0",
	"R24_UNORM_X8_TYPELESS\0",
	"X24_TYPELESS_G8_UINT\0",
	"R8G8_TYPELESS\0",
	"R8G8_UNORM\0",
	"R8G8_UINT\0",
	"R8G8_SNORM\0",
	"R8G8_SINT\0",
	"R16_TYPELESS\0",
	"R16_FLOAT\0",
	"D16_UNORM\0",
	"R16_UNORM\0",
	"R16_UINT\0",
	"R16_SNORM\0",
	"R16_SINT\0",
	"R8_TYPELESS\0",
	"R8_UNORM\0",
	"R8_UINT\0",
	"R8_SNORM\0",
	"R8_SINT\0",
	"A8_UNORM\0",
	"R1_UNORM\0",
	"R9G9B9E5_SHAREDEXP\0",
	"R8G8_B8G8_UNORM\0",
	"G8R8_G8B8_UNORM\0",
	"BC1_TYPELESS\0",
	"BC1_UNORM\0",
	"BC1_UNORM_SRGB\0",
	"BC2_TYPELESS\0",
	"BC2_UNORM\0",
	"BC2_UNORM_SRGB\0",
	"BC3_TYPELESS\0",
	"BC3_UNORM\0",
	"BC3_UNORM_SRGB\0",
	"BC4_TYPELESS\0",
	"BC4_UNORM\0",
	"BC4_SNORM\0",
	"BC5_TYPELESS\0",
	"BC5_UNORM\0",
	"BC5_SNORM\0",
	"B5G6R5_UNORM\0",
	"B5G5R5A1_UNORM\0",
	"B8G8R8A8_UNORM\0",
	"B8G8R8X8_UNORM\0",
	"R10G10B10_XR_BIAS_A2_UNORM\0",
	"B8G8R8A8_TYPELESS\0",
	"B8G8R8A8_UNORM_SRGB\0",
	"B8G8R8X8_TYPELESS\0",
	"B8G8R8X8_UNORM_SRGB\0",
	"BC6H_TYPELESS\0",
	"BC6H_UF16\0",
	"BC6H_SF16\0",
	"BC7_TYPELESS\0",
	"BC7_UNORM\0",
	"BC7_UNORM_SRGB\0",
	"AYUV\0",
	"Y410\0",
	"Y416\0",
	"NV12\0",
	"P010\0",
	"P016\0",
	"420_OPAQUE\0",
	"YUY2\0",
	"Y210\0",
	"Y216\0",
	"NV11\0",
	"AI44\0",
	"IA44\0",
	"P8\0",
	"A8P8\0",
	"B4G4R4A4_UNORM\0",
	"P208\0",
	"V208\0",
	"V408\0"
};
static const char* SYSTEM_VARIABLE_NAMES[] = {
	"--NONE--",
	"Time",
	"TimeDelta",
	"ViewportSize",
	"MousePosition",
	"View",
	"Projection",
	"ViewProjection"
};
static const char* VARIABLE_TYPE_NAMES[] = {
	"bool",
	"bool2",
	"bool3",
	"bool4",
	"int",
	"int2",
	"int3",
	"int4",
	"float",
	"float2",
	"float3",
	"float4",
	"float2x2",
	"float3x3",
	"float4x4"
};

#define PIPELINE_ITEM_INDENT 65

namespace ed
{
	void PipelineUI::OnEvent(const ml::Event & e)
	{}
	void PipelineUI::Update(float delta)
	{
		std::vector<ed::PipelineManager::Item>& items = m_data->Pipeline.GetList();

		for (int i = 0; i < items.size(); i++) {
			m_renderItemUpDown(items, i);
			if (items[i].Type == ed::PipelineItem::ShaderFile)
				m_addShader(items[i].Name, (ed::pipe::ShaderItem*)items[i].Data);
			else
				m_addItem(items[i].Name);
			m_renderItemContext(items, i);
		}



		// various popups
		if (m_isLayoutOpened) {
			ImGui::OpenPopup("Item Manager##pui_layout_items");
			m_isLayoutOpened = false;
		}
		if (m_isVarManagerOpened) {
			ImGui::OpenPopup("Variable Manager##pui_shader_variables");
			m_isVarManagerOpened = false;
		}

		// Input Layout item manager
		ImGui::SetNextWindowSize(ImVec2(600, 175), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Item Manager##pui_layout_items")) {
			m_renderInputLayoutUI();

			if (ImGui::Button("Ok")) m_closePopup();
			ImGui::EndPopup();
		}

		// Shader Variable manager
		ImGui::SetNextWindowSize(ImVec2(730, 175), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Variable Manager##pui_shader_variables")) {
			m_renderVariableManagerUI();

			if (ImGui::Button("Ok")) m_closePopup();
			ImGui::EndPopup();
		}
	}
	
	void PipelineUI::m_closePopup()
	{
		ImGui::CloseCurrentPopup();
		m_modalItem = nullptr;
	}
	
	void PipelineUI::m_renderItemUpDown(std::vector<ed::PipelineManager::Item>& items, int index)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::ArrowButton(std::string("##U" + std::string(items[index].Name)).c_str(), ImGuiDir_Up)) {
			if (index != 0) {
				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")));
				std::string oldPropertyItemName = "";
				if (props->HasItemSelected())
					oldPropertyItemName = props->CurrentItemName();

				ed::PipelineManager::Item temp = items[index - 1];
				items[index - 1] = items[index];
				items[index] = temp;

				if (props->HasItemSelected()) {
					if (oldPropertyItemName == items[index - 1].Name)
						props->Open(&items[index - 1]);
					if (oldPropertyItemName == items[index].Name)
						props->Open(&items[index]);
				}
			}
		}
		ImGui::SameLine(0, 0);

		if (ImGui::ArrowButton(std::string("##D" + std::string(items[index].Name)).c_str(), ImGuiDir_Down)) {
			if (index != items.size() - 1) {
				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")));
				std::string oldPropertyItemName = "";
				if (props->HasItemSelected())
					oldPropertyItemName = props->CurrentItemName();

				ed::PipelineManager::Item temp = items[index + 1];
				items[index + 1] = items[index];
				items[index] = temp;

				if (props->HasItemSelected()) {
					if (oldPropertyItemName == items[index + 1].Name)
						props->Open(&items[index + 1]);
					if (oldPropertyItemName == items[index].Name)
						props->Open(&items[index]);
				}
			}
		}
		ImGui::SameLine(0, 0);

		ImGui::PopStyleColor();
	}
	void PipelineUI::m_renderItemContext(std::vector<ed::PipelineManager::Item>& items, int index)
	{
		if (ImGui::BeginPopupContextItem(("##context_" + std::string(items[index].Name)).c_str())) {
			if (items[index].Type == ed::PipelineItem::ShaderFile) {
				if (ImGui::Selectable("Recompile"))
					m_data->Renderer.Recompile(items[index].Name);

				if (((ed::pipe::ShaderItem*)items[index].Data)->Type == ed::pipe::ShaderItem::VertexShader) {
					if (ImGui::Selectable("Items...")) {
						m_isLayoutOpened = true;
						m_modalItem = &items[index];
					}
				}
				if (ImGui::Selectable("Variables")) {
					m_isVarManagerOpened = true;
					m_modalItem = &items[index];
				}
			}

			if (ImGui::Selectable("Properties"))
				(reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")))->Open(&items[index]);

			if (ImGui::Selectable("Delete")) {
				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")));
				if (props->HasItemSelected() && props->CurrentItemName() == items[index].Name)
					props->Open(nullptr);

				m_data->Pipeline.Remove(items[index].Name);
			}

			ImGui::EndPopup();
		}
	}
	void PipelineUI::m_renderInputLayoutUI()
	{
		static D3D11_INPUT_ELEMENT_DESC iElement = {
			(char*)calloc(SEMANTIC_LENGTH, sizeof(char)), 0, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0
		};
		static bool scrollToBottom = false;

		ImGui::Text("Add or remove items from vertex input layout.");

		ImGui::BeginChild("##pui_layout_table", ImVec2(0, -25));
		ImGui::Columns(5);

		ImGui::Text("Semantic"); ImGui::NextColumn();
		ImGui::Text("Semantic ID"); ImGui::NextColumn();
		ImGui::Text("Format"); ImGui::NextColumn();
		ImGui::Text("Offset"); ImGui::NextColumn();
		ImGui::Text("Controls"); ImGui::NextColumn();

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

		int id = 0;
		std::vector<D3D11_INPUT_ELEMENT_DESC>& els = reinterpret_cast<ed::pipe::ShaderItem*>(m_modalItem->Data)->InputLayout.GetInputElements();
		for (auto& el : els) {
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
				ImGui::InputText(("##semantic" + std::to_string(id)).c_str(), const_cast<char*>(el.SemanticName), SEMANTIC_LENGTH);
			ImGui::NextColumn();
			
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
				ImGui::InputInt(("##semanticID" + std::to_string(id)).c_str(), reinterpret_cast<int*>(&el.SemanticIndex));
			ImGui::NextColumn();
			
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
				ImGui::Combo(("##formatName" + std::to_string(id)).c_str(), reinterpret_cast<int*>(&el.Format), FORMAT_NAMES, ARRAYSIZE(FORMAT_NAMES));
			ImGui::NextColumn();
			
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
				ImGui::InputInt(("##byteOffset" + std::to_string(id)).c_str(), reinterpret_cast<int*>(&el.AlignedByteOffset));
			ImGui::NextColumn();


			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				if (ImGui::Button(("U##" + std::to_string(id)).c_str()) && id != 0) {
					D3D11_INPUT_ELEMENT_DESC temp = els[id - 1];
					els[id - 1] = el;
					els[id] = temp;
				}
				ImGui::SameLine(); if (ImGui::Button(("D##" + std::to_string(id)).c_str()) && id != els.size() - 1) {
					D3D11_INPUT_ELEMENT_DESC temp = els[id + 1];
					els[id + 1] = el;
					els[id] = temp;
				}
				ImGui::SameLine(); if (ImGui::Button(("X##" + std::to_string(id)).c_str()))
					els.erase(els.begin() + id);
			ImGui::PopStyleColor();
			ImGui::NextColumn();

			id++;
		}

		ImGui::PopStyleColor();

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::InputText("##inputSemantic", const_cast<char*>(iElement.SemanticName), SEMANTIC_LENGTH); 
		ImGui::NextColumn();

		if (scrollToBottom) {
			ImGui::SetScrollHere();
			scrollToBottom = false;
		}

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::InputInt("##inputSemanticID", reinterpret_cast<int*>(&iElement.SemanticIndex));
		ImGui::NextColumn();

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::Combo("##inputFormatName", reinterpret_cast<int*>(&iElement.Format), FORMAT_NAMES, ARRAYSIZE(FORMAT_NAMES));
		ImGui::NextColumn();

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::InputInt("##inputByteOffset", reinterpret_cast<int*>(&iElement.AlignedByteOffset));
		ImGui::NextColumn();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			if (ImGui::Button("ADD")) {
				els.push_back(iElement);

				iElement.SemanticName = (char*)calloc(SEMANTIC_LENGTH, sizeof(char));
				iElement.SemanticIndex = 0;
				
				scrollToBottom = true;
			}
		ImGui::NextColumn();
		ImGui::PopStyleColor();
		//ImGui::PopItemWidth();

		ImGui::EndChild();
		ImGui::Columns(1);
	}
	void PipelineUI::m_renderVariableManagerUI()
	{
		static ed::ShaderVariable iVariable(ed::ShaderVariable::ValueType::Float1, "var", ed::SystemShaderVariable::None, 0);
		static ed::ShaderVariable::ValueType iValueType = ed::ShaderVariable::ValueType::Float1;
		static bool scrollToBottom = false;
		
		ed::pipe::ShaderItem* itemData = reinterpret_cast<ed::pipe::ShaderItem*>(m_modalItem->Data);

		ImGui::Text("Add or remove variables bound to this shader.");

		ImGui::BeginChild("##pui_variable_table", ImVec2(0, -25));
		ImGui::Columns(5);

		ImGui::Text("Type"); ImGui::NextColumn();
		ImGui::Text("Name"); ImGui::NextColumn();
		ImGui::Text("System"); ImGui::NextColumn();
		ImGui::Text("Slot"); ImGui::NextColumn();
		ImGui::Text("Controls"); ImGui::NextColumn();

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

		int id = 0;
		std::vector<ed::ShaderVariable>& els = itemData->Variables.GetVariables();
		for (auto& el : els) {
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::Text(VARIABLE_TYPE_NAMES[(int)el.GetType()]);
			ImGui::NextColumn();

			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::InputText(("##name" + std::to_string(id)).c_str(), const_cast<char*>(el.Name), VARIABLE_NAME_LENGTH);
			ImGui::NextColumn();

			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			const char* systemComboPreview = SYSTEM_VARIABLE_NAMES[(int)el.System];
			if (ImGui::BeginCombo(("##system" + std::to_string(id)).c_str(), systemComboPreview)) {
				for (int n = 0; n < _ARRAYSIZE(SYSTEM_VARIABLE_NAMES); n++) {
					bool is_selected = (n == (int)el.System);
					if ((n == 0 || ed::SystemVariableManager::GetType((ed::SystemShaderVariable)n) == el.GetType())
						&& ImGui::Selectable(SYSTEM_VARIABLE_NAMES[n], is_selected))
							el.System = (ed::SystemShaderVariable)n;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::NextColumn();

			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::DragInt(("##slot" + std::to_string(id)).c_str(), &el.Slot, 0.1f, 0, CONSTANT_BUFFER_SLOTS-1, "b%d");
			el.Slot = std::min<int>(CONSTANT_BUFFER_SLOTS - 1, el.Slot);
			ImGui::NextColumn();


			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

			if (el.System == ed::SystemShaderVariable::None) {
				if (ImGui::Button(("EDIT##" + std::to_string(id)).c_str())) {
					ImGui::OpenPopup("Value Edit##pui_shader_value_edit");
					m_valueEdit.Open(&el);
				}
				ImGui::SameLine();

				PinnedUI* pinState = ((PinnedUI*)m_ui->Get("Pinned"));
				if (!pinState->Contains(el.Name)) {
					if (ImGui::Button(("PIN##" + std::to_string(id)).c_str()))
						pinState->Add(&el);
				} else {
					if (ImGui::Button(("UNPIN##" + std::to_string(id)).c_str()))
						pinState->Remove(el.Name);
				}

				ImGui::SameLine();
			}
			if (ImGui::Button(("U##" + std::to_string(id)).c_str()) && id != 0) {
				// check if any of the affected variables are pinned
				PinnedUI* pinState = ((PinnedUI*)m_ui->Get("Pinned"));
				bool containsCur = pinState->Contains(el.Name);
				bool containsDown = pinState->Contains(els[id-1].Name);

				// first unpin if it was pinned
				if (containsCur)
					pinState->Remove(el.Name);
				if (containsDown)
					pinState->Remove(els[id - 1].Name);

				ed::ShaderVariable temp = els[id - 1];
				els[id - 1] = el;
				els[id] = temp;

				// then pin again if it was previously pinned
				if (containsCur)
					pinState->Add(&els[id-1]);
				if (containsDown)
					pinState->Add(&els[id]);
			}
			ImGui::SameLine(); if (ImGui::Button(("D##" + std::to_string(id)).c_str()) && id != els.size() - 1) {
				// check if any of the affected variables are pinned
				PinnedUI* pinState = ((PinnedUI*)m_ui->Get("Pinned"));
				bool containsCur = pinState->Contains(el.Name);
				bool containsDown = pinState->Contains(els[id + 1].Name);

				// first unpin if it was pinned
				if (containsCur)
					pinState->Remove(el.Name);
				if (containsDown)
					pinState->Remove(els[id + 1].Name);

				ed::ShaderVariable temp = els[id + 1];
				els[id + 1] = el;
				els[id] = temp;

				// then pin again if it was previously pinned
				if (containsCur)
					pinState->Add(&els[id + 1]);
				if (containsDown)
					pinState->Add(&els[id]);
			}
			ImGui::SameLine(); if (ImGui::Button(("X##" + std::to_string(id)).c_str()))
				itemData->Variables.Remove(el.Name);

			ImGui::PopStyleColor();
			ImGui::NextColumn();

			id++;
		}

		ImGui::PopStyleColor();

		// render value edit window if needed
		ImGui::SetNextWindowSize(ImVec2(450, 175), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Value Edit##pui_shader_value_edit")) {
			m_valueEdit.Update();
			
			if (ImGui::Button("Ok")) {
				m_valueEdit.Close();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// widgets for editing "virtual" element - an element that will be added to the list later
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::Combo("##inputType", reinterpret_cast<int*>(&iValueType), VARIABLE_TYPE_NAMES, ARRAYSIZE(VARIABLE_TYPE_NAMES))) {
			if (iValueType != iVariable.GetType()) {
				ed::ShaderVariable newVariable(iValueType);
				memcpy(newVariable.Name, iVariable.Name, strlen(iVariable.Name));
				newVariable.Slot = iVariable.Slot;
				newVariable.System = ed::SystemShaderVariable::None;
				memcpy(newVariable.Data, iVariable.Data, std::min<int>(ed::ShaderVariable::GetSize(iVariable.GetType()), ed::ShaderVariable::GetSize(newVariable.GetType())));

				free(iVariable.Data);
				iVariable = newVariable;
			}
		}
		ImGui::NextColumn();

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		ImGui::InputText("##inputName", const_cast<char*>(iVariable.Name), SEMANTIC_LENGTH);
		ImGui::NextColumn();

		if (scrollToBottom) {
			ImGui::SetScrollHere();
			scrollToBottom = false;
		}

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		const char* inputComboPreview = SYSTEM_VARIABLE_NAMES[(int)iVariable.System];
		if (ImGui::BeginCombo(("##system" + std::to_string(id)).c_str(), inputComboPreview)) {
			for (int n = 0; n < (int)SystemShaderVariable::Count; n++) {
				bool is_selected = (n == (int)iVariable.System);
				if ((n == 0 || ed::SystemVariableManager::GetType((ed::SystemShaderVariable)n) == iVariable.GetType())
					&& ImGui::Selectable(SYSTEM_VARIABLE_NAMES[n], is_selected))
						iVariable.System = (ed::SystemShaderVariable)n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::NextColumn();

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		ImGui::DragInt("##inputSlot", &iVariable.Slot, 0.1f, 0, CONSTANT_BUFFER_SLOTS - 1, "b%d");
		iVariable.Slot = std::min<int>(CONSTANT_BUFFER_SLOTS - 1, iVariable.Slot);
		ImGui::NextColumn();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::Button("ADD")) {
			// cant have two variables with same name
			bool exists = false;
			for (auto el : els)
				if (strcmp(el.Name, iVariable.Name) == 0)
					exists = true;

			// add if it doesnt exist
			if (!exists) {
				itemData->Variables.Add(iVariable);
				iVariable = ShaderVariable(ShaderVariable::ValueType::Float1, "var", ed::SystemShaderVariable::None, 0);
				iValueType = ShaderVariable::ValueType::Float1;
				scrollToBottom = true;
			}
		}
		ImGui::NextColumn();
		ImGui::PopStyleColor();
		//ImGui::PopItemWidth();

		ImGui::EndChild();
		ImGui::Columns(1);
	}

	void PipelineUI::m_addShader(const std::string& name, ed::pipe::ShaderItem* data)
	{
		std::string type = "PS";
		if (data->Type == ed::pipe::ShaderItem::VertexShader)
			type = "VS";

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		if (ImGui::SmallButton(type.c_str()))
			m_data->Renderer.Recompile(name.c_str());
		ImGui::PopStyleColor();

		ImGui::SameLine();

		ImGui::Indent(PIPELINE_ITEM_INDENT);
		ImGui::Selectable(name.c_str());
		ImGui::Unindent(PIPELINE_ITEM_INDENT);
	}
	void PipelineUI::m_addItem(const std::string & name)
	{
		ImGui::Indent(PIPELINE_ITEM_INDENT);
		ImGui::Selectable(name.c_str());
		ImGui::Unindent(PIPELINE_ITEM_INDENT);
	}
}