#include "PipelineUI.h"
#include "CodeEditorUI.h"
#include "PropertyUI.h"
#include "PinnedUI.h"
#include "../Options.h"
#include "../GUIManager.h"
#include "../ImGUI/imgui.h"
#include "../Objects/Names.h"
#include "../Objects/GLSL2HLSL.h"
#include "../Objects/SystemVariableManager.h"
#include "../ImGUI/imgui_internal.h"

#include <algorithm>

#define PIPELINE_SHADER_PASS_INDENT 65
#define PIPELINE_ITEM_INDENT 75

namespace ed
{
	void PipelineUI::OnEvent(const ml::Event & e)
	{}
	void PipelineUI::Update(float delta)
	{
		std::vector<ed::PipelineItem*>& items = m_data->Pipeline.GetList();

		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
		for (int i = 0; i < items.size(); i++) {
			m_renderItemUpDown(items, i);
			m_addShaderPass(items[i]);
			if (m_renderItemContext(items, i)) {
				i--;
				continue;
			}

			ed::pipe::ShaderPass* data = (ed::pipe::ShaderPass*)items[i]->Data;

			bool showItems = true;
			for (int j = 0; j < m_expandList.size(); j++)
				if (m_expandList[j] == data) {
					showItems = false;
					break;
				}

			if (showItems) {
				for (int j = 0; j < data->Items.size(); j++) {
					m_renderItemUpDown(data->Items, j);
					m_addItem(data->Items[j]);
					if (m_renderItemContext(data->Items, j)) {
						j--;
						continue;
					}
				}
			}
		}
		ImGui::PopStyleVar();


		// various popups
		if (m_isLayoutOpened) {
			ImGui::OpenPopup("Vertex Input Layout##pui_layout_items");
			m_isLayoutOpened = false;
		}
		if (m_isVarManagerOpened) {
			ImGui::OpenPopup("Variable Manager##pui_shader_variables");
			m_isVarManagerOpened = false;
		}
		if (m_isCreateViewOpened) {
			ImGui::OpenPopup("Create Item##pui_create_item");
			m_isCreateViewOpened = false;
		}
		if (m_isChangeVarsOpened) {
			ImGui::OpenPopup("Change Variables##pui_render_variables");
			m_isChangeVarsOpened = false;
		}

		// VS Input Layout
		ImGui::SetNextWindowSize(ImVec2(600, 175), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Vertex Input Layout##pui_layout_items")) {
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

		// Create Item
		ImGui::SetNextWindowSize(ImVec2(430, 175), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Create Item##pui_create_item")) {
			m_createUI.Update(delta);

			if (ImGui::Button("Ok")) {
				if (m_createUI.Create())
					m_closePopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) m_closePopup();
			ImGui::EndPopup();
		}

		// variables to change when rendering specific item
		ImGui::SetNextWindowSize(ImVec2(400, 175), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Change Variables##pui_render_variables")) {
			m_renderChangeVariablesUI();

			if (ImGui::Button("Ok")) m_closePopup();
			ImGui::EndPopup();
		}
	}
	
	void PipelineUI::m_renderItemUpDown(std::vector<ed::PipelineItem*>& items, int index)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::ArrowButton(std::string("##U" + std::string(items[index]->Name)).c_str(), ImGuiDir_Up)) {
			if (index != 0) {
				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")));
				std::string oldPropertyItemName = "";
				if (props->HasItemSelected())
					oldPropertyItemName = props->CurrentItemName();

				ed::PipelineItem* temp = items[index - 1];
				items[index - 1] = items[index];
				items[index] = temp;

				if (props->HasItemSelected()) {
					if (oldPropertyItemName == items[index - 1]->Name)
						props->Open(items[index - 1]);
					if (oldPropertyItemName == items[index]->Name)
						props->Open(items[index]);
				}
			}
		}
		ImGui::SameLine(0, 0);

		if (ImGui::ArrowButton(std::string("##D" + std::string(items[index]->Name)).c_str(), ImGuiDir_Down)) {
			if (index != items.size() - 1) {
				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")));
				std::string oldPropertyItemName = "";
				if (props->HasItemSelected())
					oldPropertyItemName = props->CurrentItemName();

				ed::PipelineItem* temp = items[index + 1];
				items[index + 1] = items[index];
				items[index] = temp;

				if (props->HasItemSelected()) {
					if (oldPropertyItemName == items[index + 1]->Name)
						props->Open(items[index + 1]);
					if (oldPropertyItemName == items[index]->Name)
						props->Open(items[index]);
				}
			}
		}
		ImGui::SameLine(0, 0);

		ImGui::PopStyleColor();
	}
	bool PipelineUI::m_renderItemContext(std::vector<ed::PipelineItem*>& items, int index)
	{
		bool ret = false; // false == we didnt delete an item

		if (ImGui::BeginPopupContextItem(("##context_" + std::string(items[index]->Name)).c_str())) {
			if (items[index]->Type == ed::PipelineItem::ItemType::ShaderPass) {
				if (ImGui::Selectable("Recompile"))
					m_data->Renderer.Recompile(items[index]->Name);

				if (ImGui::BeginMenu("Add")) {
					if (ImGui::MenuItem("Geometry")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::Geometry);
					}
					else if (ImGui::MenuItem(".obj model")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::OBJModel);
					}
					else if (ImGui::MenuItem("Blend State")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::BlendState);
					}
					else if (ImGui::MenuItem("Depth Stencil State")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::DepthStencilState);
					}
					else if (ImGui::MenuItem("Rasterizer State")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::RasterizerState);
					}

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Edit Code")) {
					pipe::ShaderPass* passData = (pipe::ShaderPass*)(items[index]->Data);

					if (ImGui::MenuItem("Vertex Shader"))
						(reinterpret_cast<CodeEditorUI*>(m_ui->Get("Code")))->OpenVS(*items[index]);
					else if (ImGui::MenuItem("Pixel Shader"))
						(reinterpret_cast<CodeEditorUI*>(m_ui->Get("Code")))->OpenPS(*items[index]);
					else if (passData->GSUsed && ImGui::MenuItem("Geometry Shader"))
						(reinterpret_cast<CodeEditorUI*>(m_ui->Get("Code")))->OpenGS(*items[index]);
					else if (ImGui::MenuItem("All")) {
						if (passData->GSUsed)
							(reinterpret_cast<CodeEditorUI*>(m_ui->Get("Code")))->OpenGS(*items[index]);
						(reinterpret_cast<CodeEditorUI*>(m_ui->Get("Code")))->OpenPS(*items[index]);
						(reinterpret_cast<CodeEditorUI*>(m_ui->Get("Code")))->OpenVS(*items[index]);
					}

					ImGui::EndMenu();
				}

				if (ImGui::Selectable("Input Layout")) {
					m_isLayoutOpened = true;
					m_modalItem = items[index];
				}

				if (ImGui::BeginMenu("Variables")) {
					if (ImGui::MenuItem("Vertex Shader")) {
						m_isVarManagerOpened = true;
						m_VarManagerSID = 0;
						m_modalItem = items[index];
					}
					else if (ImGui::MenuItem("Pixel Shader")) {
						m_isVarManagerOpened = true;
						m_VarManagerSID = 1;
						m_modalItem = items[index];
					}
					else if (ImGui::MenuItem("Geometry Shader")) {
						m_isVarManagerOpened = true;
						m_VarManagerSID = 2;
						m_modalItem = items[index];
					}

					ImGui::EndMenu();
				}
			}
			else if (items[index]->Type == ed::PipelineItem::ItemType::Geometry || items[index]->Type == ed::PipelineItem::ItemType::OBJModel) {
				if (ImGui::MenuItem("Change Variables")) {
					m_isChangeVarsOpened = true;
					m_modalItem = items[index];
				}
			}

			if (ImGui::Selectable("Properties"))
				(reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")))->Open(items[index]);

			if (ImGui::Selectable("Delete")) {

				// check if it is opened in property viewer
				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")));
				if (props->HasItemSelected() && props->CurrentItemName() == items[index]->Name)
					props->Open(nullptr);

				// tell pipeline to remove this item
				m_data->Renderer.RemoveItemVariableValues(items[index]);
				m_data->Pipeline.Remove(items[index]->Name);

				ret = true;
			}

			ImGui::EndPopup();

			return ret;
		}

		return ret;
	}

	void PipelineUI::m_closePopup()
	{
		ImGui::CloseCurrentPopup();
		m_modalItem = nullptr;
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
		pipe::ShaderPass* shaderPass = reinterpret_cast<pipe::ShaderPass*>(m_modalItem->Data);
		std::vector<D3D11_INPUT_ELEMENT_DESC>& els = shaderPass->VSInputLayout.GetInputElements();

		bool isGLSL = GLSL2HLSL::IsGLSL(shaderPass->VSPath);

		for (auto& el : els) {
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
				if (isGLSL) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}
				ImGui::InputText(("##semantic" + std::to_string(id)).c_str(), const_cast<char*>(el.SemanticName), SEMANTIC_LENGTH);
				if (isGLSL) {
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}
			ImGui::NextColumn();
			
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
				ImGui::InputInt(("##semanticID" + std::to_string(id)).c_str(), reinterpret_cast<int*>(&el.SemanticIndex));
			ImGui::NextColumn();
			
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
				ImGui::Combo(("##formatName" + std::to_string(id)).c_str(), reinterpret_cast<int*>(&el.Format), FORMAT_NAMES, _ARRAYSIZE(FORMAT_NAMES));
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
				if (isGLSL) {
					strcpy(const_cast<char*>(iElement.SemanticName), "TEXCOORD");
					iElement.SemanticIndex = els.size();
				}

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
		
		ed::pipe::ShaderPass* itemData = reinterpret_cast<ed::pipe::ShaderPass*>(m_modalItem->Data);

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
		std::vector<ed::ShaderVariable*>& els = m_VarManagerSID == 0 ? itemData->VSVariables.GetVariables() :
			(m_VarManagerSID == 1 ? itemData->PSVariables.GetVariables() : itemData->GSVariables.GetVariables());

		for (auto& el : els) {
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::Text(VARIABLE_TYPE_NAMES[(int)el->GetType()]);
			ImGui::NextColumn();

			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::InputText(("##name" + std::to_string(id)).c_str(), const_cast<char*>(el->Name), VARIABLE_NAME_LENGTH);
			ImGui::NextColumn();

			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			const char* systemComboPreview = SYSTEM_VARIABLE_NAMES[(int)el->System];
			if (ImGui::BeginCombo(("##system" + std::to_string(id)).c_str(), systemComboPreview)) {
				for (int n = 0; n < _ARRAYSIZE(SYSTEM_VARIABLE_NAMES); n++) {
					bool is_selected = (n == (int)el->System);
					if ((n == 0 || ed::SystemVariableManager::GetType((ed::SystemShaderVariable)n) == el->GetType())
						&& ImGui::Selectable(SYSTEM_VARIABLE_NAMES[n], is_selected))
							el->System = (ed::SystemShaderVariable)n;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::NextColumn();

			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::DragInt(("##slot" + std::to_string(id)).c_str(), &el->Slot, 0.1f, 0, CONSTANT_BUFFER_SLOTS-1, "b%d");
			el->Slot = std::min<int>(CONSTANT_BUFFER_SLOTS - 1, el->Slot);
			ImGui::NextColumn();


			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

			if (el->System == ed::SystemShaderVariable::None) {
				if (ImGui::Button(("EDIT##" + std::to_string(id)).c_str())) {
					ImGui::OpenPopup("Value Edit##pui_shader_value_edit");
					m_valueEdit.Open(el);
				}
				ImGui::SameLine();

				PinnedUI* pinState = ((PinnedUI*)m_ui->Get("Pinned"));
				if (!pinState->Contains(el->Name)) {
					if (ImGui::Button(("PIN##" + std::to_string(id)).c_str()))
						pinState->Add(el);
				} else if (ImGui::Button(("UNPIN##" + std::to_string(id)).c_str()))
						pinState->Remove(el->Name);

				ImGui::SameLine();
			}
			if (ImGui::Button(("U##" + std::to_string(id)).c_str()) && id != 0) {
				// check if any of the affected variables are pinned
				PinnedUI* pinState = ((PinnedUI*)m_ui->Get("Pinned"));
				bool containsCur = pinState->Contains(el->Name);
				bool containsDown = pinState->Contains(els[id-1]->Name);

				// first unpin if it was pinned
				if (containsCur)
					pinState->Remove(el->Name);
				if (containsDown)
					pinState->Remove(els[id - 1]->Name);

				ed::ShaderVariable* temp = els[id - 1];
				els[id - 1] = el;
				els[id] = temp;

				// then pin again if it was previously pinned
				if (containsCur)
					pinState->Add(els[id-1]);
				if (containsDown)
					pinState->Add(els[id]);
			}
			ImGui::SameLine();
			if (ImGui::Button(("D##" + std::to_string(id)).c_str()) && id != els.size() - 1) {
				// check if any of the affected variables are pinned
				PinnedUI* pinState = ((PinnedUI*)m_ui->Get("Pinned"));
				bool containsCur = pinState->Contains(el->Name);
				bool containsDown = pinState->Contains(els[id + 1]->Name);

				// first unpin if it was pinned
				if (containsCur)
					pinState->Remove(el->Name);
				if (containsDown)
					pinState->Remove(els[id + 1]->Name);

				ed::ShaderVariable* temp = els[id + 1];
				els[id + 1] = el;
				els[id] = temp;

				// then pin again if it was previously pinned
				if (containsCur)
					pinState->Add(els[id + 1]);
				if (containsDown)
					pinState->Add(els[id]);
			}
			ImGui::SameLine();
			if (ImGui::Button(("X##" + std::to_string(id)).c_str())) {
				((PinnedUI*)m_ui->Get("Pinned"))->Remove(el->Name); // unpin if pinned

				if (m_VarManagerSID == 0)
					itemData->VSVariables.Remove(el->Name);
				else if (m_VarManagerSID == 1)
					itemData->PSVariables.Remove(el->Name);
				else if (m_VarManagerSID == 2)
					itemData->GSVariables.Remove(el->Name);
			}

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
				if (strcmp(el->Name, iVariable.Name) == 0)
					exists = true;

			// add if it doesnt exist
			if (!exists) {
				if (m_VarManagerSID == 0)
					itemData->VSVariables.AddCopy(iVariable);
				else if (m_VarManagerSID == 1)
					itemData->PSVariables.AddCopy(iVariable);
				else if (m_VarManagerSID == 2)
					itemData->GSVariables.AddCopy(iVariable);
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
	void PipelineUI::m_renderChangeVariablesUI()
	{
		static bool scrollToBottom = false;
		static int shaderTypeSel = 0, shaderVarSel = 0;
		
		ImGui::Text("Add variables that you want to change when rendering this item");

		ImGui::BeginChild("##pui_cvar_table", ImVec2(0, -25));
		ImGui::Columns(3);

		ImGui::Text("From"); ImGui::NextColumn();
		ImGui::Text("Name"); ImGui::NextColumn();
		ImGui::Text("Value"); ImGui::NextColumn();

		ImGui::Separator();

		// find this item's owner
		PipelineItem* owner = nullptr;
		pipe::ShaderPass* ownerData = nullptr;
		std::vector<PipelineItem*>& passes = m_data->Pipeline.GetList();
		for (PipelineItem* pass : passes) {
			ownerData = ((pipe::ShaderPass*)pass->Data);
			std::vector<PipelineItem*>& children = ownerData->Items;
			for (PipelineItem* child : children) {
				if (child == m_modalItem) {
					owner = pass;
					break;
				}
			}
			if (owner != nullptr)
				break;
		}

		std::vector<ShaderVariable*>& vsVars = ownerData->VSVariables.GetVariables();
		std::vector<ShaderVariable*>& psVars = ownerData->PSVariables.GetVariables();
		std::vector<ShaderVariable*>& gsVars = ownerData->GSVariables.GetVariables();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		// render the list
		int id = 0;
		std::vector<RenderEngine::ItemVariableValue> allItems = m_data->Renderer.GetItemVariableValues();
		for (auto& i : allItems) {
			if (i.Item != m_modalItem)
				continue;

			int sid = 0;
			for (auto& var : psVars)
				if (var == i.Variable) {
					sid = 1;
					break;
				}
			for (auto& var : gsVars)
				if (var == i.Variable) {
					sid = 2;
					break;
				}

			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			if (sid == 0) ImGui::Text("vs"); else if (sid == 1) ImGui::Text("ps"); else if (sid == 2) ImGui::Text("gs");
			ImGui::NextColumn();

			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::Text(i.Variable->Name);
			ImGui::NextColumn();

			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			if (ImGui::Button(("EDIT##edBtn" + std::to_string(id)).c_str())) {
				ImGui::OpenPopup("Value Edit##pui_shader_ivalue_edit");
				m_valueEdit.Open(i.NewValue);
			}
			ImGui::SameLine();
			if (ImGui::Button(("X##delBtn" + std::to_string(id)).c_str()))
				m_data->Renderer.RemoveItemVariableValue(i.Item, i.Variable);
			ImGui::NextColumn();

			id++;
		}

		ImGui::PopStyleColor();

		// render value edit window if needed
		ImGui::SetNextWindowSize(ImVec2(450, 175), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Value Edit##pui_shader_ivalue_edit")) {
			m_valueEdit.Update();

			if (ImGui::Button("Ok")) {
				m_valueEdit.Close();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// widgets for editing "virtual" element - an element that will be added to the list later
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::BeginCombo(("##shadertype" + std::to_string(id)).c_str(), shaderTypeSel == 0 ? "vs" : (shaderTypeSel == 1 ? "ps" : "gs"))) {
			if (ImGui::Selectable("vs", shaderTypeSel == 0)) { shaderTypeSel = 0; shaderVarSel = 0; }
			if (shaderTypeSel == 0) ImGui::SetItemDefaultFocus();

			if (ImGui::Selectable("ps", shaderTypeSel == 1)) { shaderTypeSel = 1; shaderVarSel = 0; }
			if (shaderTypeSel == 1) ImGui::SetItemDefaultFocus();

			if (ImGui::Selectable("gs", shaderTypeSel == 2)) { shaderTypeSel = 2; shaderVarSel = 0; }
			if (shaderTypeSel == 2) ImGui::SetItemDefaultFocus();

			ImGui::EndCombo();
		}
		ImGui::NextColumn();

		if (scrollToBottom) {
			ImGui::SetScrollHere();
			scrollToBottom = false;
		}

		std::vector<ShaderVariable*> vars = vsVars;
		if (shaderTypeSel == 1) vars = psVars;
		if (shaderTypeSel == 2) vars = gsVars;

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		const char* inputComboPreview = vars.size() > 0 ? vars[shaderVarSel]->Name : "-- NONE --";
		if (ImGui::BeginCombo(("##itemvarname" + std::to_string(id)).c_str(), inputComboPreview)) {
			for (int n = 0; n < vars.size(); n++) {
				bool is_selected = (n == shaderVarSel);
				if (ImGui::Selectable(vars[n]->Name, is_selected))
					shaderVarSel = n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::NextColumn();

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::Button("ADD")) {
			bool alreadyAdded = false;
			for (int i = 0; i < allItems.size(); i++)
				if (strcmp(vars[shaderVarSel]->Name, allItems[i].Variable->Name) == 0) {
					alreadyAdded = true;
					break;
				}

			if (!alreadyAdded) {
				RenderEngine::ItemVariableValue itemVal(vars[shaderVarSel]);
				itemVal.Item = m_modalItem;

				m_data->Renderer.AddItemVariableValue(itemVal);
			}
		}
		ImGui::NextColumn();

		ImGui::EndChild();
		ImGui::Columns(1);
	}

	void PipelineUI::m_addShaderPass(ed::PipelineItem* item)
	{
		ed::pipe::ShaderPass* data = (ed::pipe::ShaderPass*)item->Data;

		std::string expandTxt = "-";
		for (int i = 0; i < m_expandList.size(); i++)
			if (m_expandList[i] == data) {
				expandTxt = "+";
				break;
			}

		expandTxt += "##passexp_" + std::string(item->Name);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		if (ImGui::SmallButton(expandTxt.c_str())) {
			if (expandTxt[0] == '+') {
				for (int i = 0; i < m_expandList.size(); i++)
					if (m_expandList[i] == data) {
						m_expandList.erase(m_expandList.begin() + i);
						break;
					}
			}
			else m_expandList.push_back(data);
		}
		ImGui::PopStyleColor();
		ImGui::SameLine();

		ImGui::Indent(PIPELINE_SHADER_PASS_INDENT);
		if (ImGui::Selectable(item->Name, false, ImGuiSelectableFlags_AllowDoubleClick))
			if (ImGui::IsMouseDoubleClicked(0)) {
				if (Settings::Instance().General.OpenShadersOnDblClk) {
					CodeEditorUI* editor = (reinterpret_cast<CodeEditorUI*>(m_ui->Get("Code")));
					editor->OpenVS(*item);
					editor->OpenPS(*item);
					if (data->GSUsed) editor->OpenGS(*item);
				} else {
					PropertyUI* props = reinterpret_cast<PropertyUI*>(m_ui->Get("Properties"));
					props->Open(item);
				}
			}
		ImGui::Unindent(PIPELINE_SHADER_PASS_INDENT);
	}
	void PipelineUI::m_addItem(ed::PipelineItem* item)
	{
		ImGui::Indent(PIPELINE_ITEM_INDENT);
		if (ImGui::Selectable(item->Name, false, ImGuiSelectableFlags_AllowDoubleClick))
			if (ImGui::IsMouseDoubleClicked(0)) {
				PropertyUI* props = reinterpret_cast<PropertyUI*>(m_ui->Get("Properties"));
				props->Open(item);
			}
		ImGui::Unindent(PIPELINE_ITEM_INDENT);
	}
}