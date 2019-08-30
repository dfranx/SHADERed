#include "PipelineUI.h"
#include "CodeEditorUI.h"
#include "PropertyUI.h"
#include "PinnedUI.h"
#include "PreviewUI.h"
#include "../Options.h"
#include "../GUIManager.h"
#include "../Objects/Names.h"
#include "../Objects/HLSL2GLSL.h"
#include "../Objects/SystemVariableManager.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <algorithm>

#define HARRAYSIZE(a) (sizeof(a)/sizeof(*a))
#define PIPELINE_SHADER_PASS_INDENT 65 * Settings::Instance().DPIScale
#define PIPELINE_ITEM_INDENT 75 * Settings::Instance().DPIScale

namespace ed
{
	void PipelineUI::OnEvent(const SDL_Event& e)
	{}
	void PipelineUI::Update(float delta)
	{
		std::vector<ed::PipelineItem*>& items = m_data->Pipeline.GetList();
		
		ImVec2 containerSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y));
		ImGui::BeginChild("##object_scroll_container", containerSize);

		if (items.size() == 0)
			ImGui::TextWrapped("Right click on this window or go to Create menu in the menu bar to create a shader pass.");

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

		ImGui::EndChild();

		if (!m_itemMenuOpened && ImGui::BeginPopupContextItem("##context_main_pipeline")) {
			if (ImGui::Selectable("Create Shader Pass")) { m_ui->CreateNewShaderPass(); }
			ImGui::EndPopup();
		}
		m_itemMenuOpened = false;

		// various popups
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
		if (m_isMacroManagerOpened) {
			ImGui::OpenPopup("Shader Macros##pui_shader_macros");
			m_isMacroManagerOpened = false;
		}

		// Shader Variable manager
		ImGui::SetNextWindowSize(ImVec2(730 * Settings::Instance().DPIScale, 175 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Variable Manager##pui_shader_variables")) {
			m_renderVariableManagerUI();

			if (ImGui::Button("Ok")) m_closePopup();
			ImGui::EndPopup();
		}

		// Shader Macro Manager
		ImGui::SetNextWindowSize(ImVec2(530 * Settings::Instance().DPIScale, 175 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Shader Macros##pui_shader_macros")) {
			m_renderMacroManagerUI();

			if (ImGui::Button("Ok")) m_closePopup();
			ImGui::EndPopup();
		}

		// Create Item
		ImGui::SetNextWindowSize(ImVec2(430 * Settings::Instance().DPIScale, 175 * Settings::Instance().DPIScale), ImGuiCond_Once);
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
		ImGui::SetNextWindowSize(ImVec2(400 * Settings::Instance().DPIScale, 175 * Settings::Instance().DPIScale), ImGuiCond_Once);
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
				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties)));
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
				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties)));
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
			m_itemMenuOpened = true;
			if (items[index]->Type == ed::PipelineItem::ItemType::ShaderPass) {
				if (ImGui::Selectable("Recompile"))
					m_data->Renderer.Recompile(items[index]->Name);

				if (ImGui::BeginMenu("Add")) {
					if (ImGui::MenuItem("Geometry")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::Geometry);
					}
					else if (ImGui::MenuItem("3D Model")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::Model);
					}
					else if (ImGui::MenuItem("Render State")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::RenderState);
					}

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Edit Code")) {
					pipe::ShaderPass* passData = (pipe::ShaderPass*)(items[index]->Data);

					// TODO: show "File doesnt exist - want to create it?" msg box 
					if (ImGui::MenuItem("Vertex Shader") && m_data->Parser.FileExists(passData->VSPath)) {
						(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->OpenVS(*items[index]);
					}
					else if (ImGui::MenuItem("Pixel Shader") && m_data->Parser.FileExists(passData->PSPath)) {
						(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->OpenPS(*items[index]);
					}
					else if (passData->GSUsed && ImGui::MenuItem("Geometry Shader") && m_data->Parser.FileExists(passData->GSPath)) {
						(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->OpenGS(*items[index]);
					}
					else if (ImGui::MenuItem("All")) {
						if (passData->GSUsed && m_data->Parser.FileExists(passData->GSPath))
							(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->OpenGS(*items[index]);

						if (m_data->Parser.FileExists(passData->PSPath))
							(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->OpenPS(*items[index]);

						if (m_data->Parser.FileExists(passData->VSPath))
							(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->OpenVS(*items[index]);
					}

					ImGui::EndMenu();
				}

				if (ImGui::MenuItem("Variables")) {
					m_isVarManagerOpened = true;
					m_modalItem = items[index];
				}

				if (ImGui::MenuItem("Macros")) {
					m_isMacroManagerOpened = true;
					m_modalItem = items[index];
				}
			}
			else if (items[index]->Type == ed::PipelineItem::ItemType::Geometry || items[index]->Type == ed::PipelineItem::ItemType::Model) {
				if (ImGui::MenuItem("Change Variables")) {
					m_isChangeVarsOpened = true;
					m_modalItem = items[index];
				}
			}

			if (ImGui::Selectable("Properties"))
				(reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties)))->Open(items[index]);

			if (ImGui::Selectable("Delete")) {

				// check if it is opened in property viewer
				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties)));
				if (props->HasItemSelected() && props->CurrentItemName() == items[index]->Name)
					props->Open(nullptr);

				if ((reinterpret_cast<PreviewUI*>(m_ui->Get(ViewID::Preview)))->IsPicked(items[index]))
					(reinterpret_cast<PreviewUI*>(m_ui->Get(ViewID::Preview)))->Pick(nullptr);

				// tell pipeline to remove this item
				m_data->Messages.ClearGroup(items[index]->Name);
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

	void PipelineUI::m_flagTooltip(const std::string& text)
	{
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(text.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}
	void PipelineUI::m_renderVarFlags(ed::ShaderVariable* var, char flags)
	{
		ShaderVariable::ValueType type = var->GetType();
		bool canInvert = type >= ShaderVariable::ValueType::Float2x2 && type <= ShaderVariable::ValueType::Float4x4;
		bool isInvert = var->Flags & (char)ShaderVariable::Flag::Inverse;
		bool isLastFrame = var->Flags & (char)ShaderVariable::Flag::LastFrame;

		if (var->System == ed::SystemShaderVariable::None || !canInvert) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		ImGui::Checkbox(("##flaginv" + std::string(var->Name)).c_str(), &isInvert);
		m_flagTooltip("Invert");
		ImGui::SameLine();

		if (!canInvert && var->System != ed::SystemShaderVariable::None) {
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}

		if (var->System == ed::SystemShaderVariable::Time) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		ImGui::Checkbox(("##flaglf" + std::string(var->Name)).c_str(), &isLastFrame);
		m_flagTooltip("Use last frame values");

		if (var->System == ed::SystemShaderVariable::None || var->System == ed::SystemShaderVariable::Time) {
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}

		var->Flags = (isInvert * (char)ShaderVariable::Flag::Inverse) |
					 (isLastFrame * (char)ShaderVariable::Flag::LastFrame);
	}
	void PipelineUI::m_renderVariableManagerUI()
	{
		static ed::ShaderVariable iVariable(ed::ShaderVariable::ValueType::Float1, "var", ed::SystemShaderVariable::None);
		static ed::ShaderVariable::ValueType iValueType = ed::ShaderVariable::ValueType::Float1;
		static bool scrollToBottom = false;
		
		ed::pipe::ShaderPass* itemData = reinterpret_cast<ed::pipe::ShaderPass*>(m_modalItem->Data);

		ImGui::TextWrapped("Add or remove variables bound to this shader pass.");

		ImGui::BeginChild("##pui_variable_table", ImVec2(0, -25));
		ImGui::Columns(5);

		ImGui::Text("Type"); ImGui::NextColumn();
		ImGui::Text("Name"); ImGui::NextColumn();
		ImGui::Text("System"); ImGui::NextColumn();
		ImGui::Text("Flags"); ImGui::NextColumn();
		ImGui::Text("Controls"); ImGui::NextColumn();

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

		int id = 0;
		std::vector<ed::ShaderVariable*>& els = itemData->Variables.GetVariables();

		/* EXISTING VARIABLES */
		for (auto& el : els) {
			/* TYPE */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::Text(VARIABLE_TYPE_NAMES[(int)el->GetType()]);
			ImGui::NextColumn();

			/* NAME */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::InputText(("##name" + std::to_string(id)).c_str(), const_cast<char*>(el->Name), VARIABLE_NAME_LENGTH);
			ImGui::NextColumn();

			/* SYSTEM VALUE */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			const char* systemComboPreview = SYSTEM_VARIABLE_NAMES[(int)el->System];
			if (ImGui::BeginCombo(("##system" + std::to_string(id)).c_str(), systemComboPreview)) {
				for (int n = 0; n < HARRAYSIZE(SYSTEM_VARIABLE_NAMES); n++) {
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

			/* FLAGS */
			m_renderVarFlags(el, el->Flags);
			ImGui::NextColumn();
			
			
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

			/* EDIT & PIN BUTTONS */
			if (el->System == ed::SystemShaderVariable::None) {
				if (ImGui::Button(("EDIT##" + std::to_string(id)).c_str())) {
					ImGui::OpenPopup("Value Edit##pui_shader_value_edit");
					m_valueEdit.Open(el);
				}
				ImGui::SameLine();

				PinnedUI* pinState = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
				if (!pinState->Contains(el->Name)) {
					if (ImGui::Button(("PIN##" + std::to_string(id)).c_str()))
						pinState->Add(el);
				} else if (ImGui::Button(("UNPIN##" + std::to_string(id)).c_str()))
						pinState->Remove(el->Name);

				ImGui::SameLine();
			}

			/* UP BUTTON */
			if (ImGui::Button(("U##" + std::to_string(id)).c_str()) && id != 0) {
				// check if any of the affected variables are pinned
				PinnedUI* pinState = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
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
			/* DOWN BUTTON */
			if (ImGui::Button(("D##" + std::to_string(id)).c_str()) && id != els.size() - 1) {
				// check if any of the affected variables are pinned
				PinnedUI* pinState = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
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
			/* DELETE BUTTON */
			if (ImGui::Button(("X##" + std::to_string(id)).c_str())) {
				((PinnedUI*)m_ui->Get(ViewID::Pinned))->Remove(el->Name); // unpin if pinned
				
				itemData->Variables.Remove(el->Name);
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
		/* TYPE */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::Combo("##inputType", reinterpret_cast<int*>(&iValueType), VARIABLE_TYPE_NAMES, HARRAYSIZE(VARIABLE_TYPE_NAMES))) {
			if (iValueType != iVariable.GetType()) {
				ed::ShaderVariable newVariable(iValueType);
				memcpy(newVariable.Name, iVariable.Name, strlen(iVariable.Name));
				newVariable.System = ed::SystemShaderVariable::None;
				memcpy(newVariable.Data, iVariable.Data, std::min<int>(ed::ShaderVariable::GetSize(iVariable.GetType()), ed::ShaderVariable::GetSize(newVariable.GetType())));

				free(iVariable.Data);
				iVariable = newVariable;
			}
		}
		ImGui::NextColumn();

		/* NAME */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		ImGui::InputText("##inputName", const_cast<char*>(iVariable.Name), SEMANTIC_LENGTH);
		ImGui::NextColumn();

		if (scrollToBottom) {
			ImGui::SetScrollHere();
			scrollToBottom = false;
		}

		/* SYSTEM */
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

		/* FLAGS */
		m_renderVarFlags(&iVariable, iVariable.Flags);
		ImGui::NextColumn();

		/* ADD BUTTON */
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
				itemData->Variables.AddCopy(iVariable);

				iVariable = ShaderVariable(ShaderVariable::ValueType::Float1, "var", ed::SystemShaderVariable::None);
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
		
		ImGui::TextWrapped("Add variables that you want to change when rendering this item");

		ImGui::BeginChild("##pui_cvar_table", ImVec2(0, -25));
		ImGui::Columns(2);

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

		std::vector<ShaderVariable*>& vars = ownerData->Variables.GetVariables();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		// render the list
		int id = 0;
		std::vector<RenderEngine::ItemVariableValue> allItems = m_data->Renderer.GetItemVariableValues();
		for (auto& i : allItems) {
			if (i.Item != m_modalItem)
				continue;

			/* NAME */
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

		if (scrollToBottom) {
			ImGui::SetScrollHere();
			scrollToBottom = false;
		}

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
		if (ImGui::Button("ADD") && vars.size() > 0) {
			bool alreadyAdded = false;
			for (int i = 0; i < allItems.size(); i++)
				if (strcmp(vars[shaderVarSel]->Name, allItems[i].Variable->Name) == 0 && allItems[i].Item == m_modalItem) {
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
	
	void PipelineUI::m_renderMacroManagerUI()
	{
		static ShaderMacro addMacro = { true, "\0", "\0" };
		static bool scrollToBottom = false;
		ed::pipe::ShaderPass* itemData = reinterpret_cast<ed::pipe::ShaderPass*>(m_modalItem->Data);

		ImGui::TextWrapped("Add or remove shader macros.");

		ImGui::BeginChild("##pui_macro_table", ImVec2(0, -25));
		ImGui::Columns(4);

		ImGui::Text("Active"); ImGui::NextColumn();
		ImGui::Text("Name"); ImGui::NextColumn();
		ImGui::Text("Value"); ImGui::NextColumn();
		ImGui::Text("Controls"); ImGui::NextColumn();

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

		int id = 0;
		std::vector<ShaderMacro>& els = itemData->Macros;

		/* EXISTING VARIABLES */
		for (auto& el : els) {
			/* ACTIVE */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::Checkbox(("##pui_mcr_act" + std::to_string(id)).c_str(), &el.Active);
			ImGui::NextColumn();

			/* NAME */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::InputText(("##mcrName" + std::to_string(id)).c_str(), el.Name, 32);
			ImGui::NextColumn();

			/* VALUE */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::InputText(("##mcrVal" + std::to_string(id)).c_str(), el.Value, 512);
			ImGui::NextColumn();


			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

			/* UP BUTTON */
			if (ImGui::Button(("U##" + std::to_string(id)).c_str()) && id != 0) {
				ed::ShaderMacro temp = els[id - 1];
				els[id - 1] = el;
				els[id] = temp;
			}
			ImGui::SameLine();
			/* DOWN BUTTON */
			if (ImGui::Button(("D##" + std::to_string(id)).c_str()) && id != els.size() - 1) {
				ed::ShaderMacro temp = els[id + 1];
				els[id + 1] = el;
				els[id] = temp;
			}
			ImGui::SameLine();
			/* DELETE BUTTON */
			if (ImGui::Button(("X##" + std::to_string(id)).c_str())) {
				els.erase(els.begin() + id);
				id--;
			}

			ImGui::PopStyleColor();
			ImGui::NextColumn();

			id++;
		}

		ImGui::PopStyleColor();

		// widgets for adding a macro
		/* ACTIVE */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		ImGui::Checkbox(("##pui_mcrActAdd" + std::to_string(id)).c_str(), &addMacro.Active);
		ImGui::NextColumn();

		/* NAME */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		ImGui::InputText(("##mcrNameAdd" + std::to_string(id)).c_str(), addMacro.Name, 32);
		ImGui::NextColumn();

		/* VALUE */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		ImGui::InputText(("##mcrValAdd" + std::to_string(id)).c_str(), addMacro.Value, 512);
		ImGui::NextColumn();
		
		if (scrollToBottom) {
			ImGui::SetScrollHere();
			scrollToBottom = false;
		}

		/* ADD BUTTON */
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::Button("ADD")) {
			// cant have two macros with same name
			bool exists = false;
			for (auto& el : els)
				if (strcmp(el.Name, addMacro.Name) == 0)
					exists = true;

			// add if it doesnt exist
			if (!exists) {
				els.push_back(addMacro);
				scrollToBottom = true;
			}
		}
		ImGui::NextColumn();
		ImGui::PopStyleColor();
		//ImGui::PopItemWidth();

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
					if (Settings::Instance().General.UseExternalEditor && m_data->Parser.GetOpenedFile() == "")
						m_ui->SaveAsProject(true);
					else {
						CodeEditorUI* editor = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
						if (m_data->Parser.FileExists(data->VSPath))
							editor->OpenVS(*item);

						if (m_data->Parser.FileExists(data->PSPath))
							editor->OpenPS(*item);

						if (data->GSUsed && strlen(data->GSPath) > 0 && m_data->Parser.FileExists(data->GSPath))
							editor->OpenGS(*item);
					}
				}
				
				if (Settings::Instance().General.ItemPropsOnDblCLk) {
					PropertyUI* props = reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties));
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
				if (Settings::Instance().General.ItemPropsOnDblCLk) {
					PropertyUI* props = reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties));
					props->Open(item);
				}

				if (Settings::Instance().General.SelectItemOnDblClk) {
					if (item->Type == PipelineItem::ItemType::Geometry ||
						item->Type == PipelineItem::ItemType::Model)
					{
						bool proceed = true;
						if (item->Type == PipelineItem::ItemType::Geometry)
							proceed = ((pipe::GeometryItem*)item->Data)->Type != pipe::GeometryItem::GeometryType::Rectangle;
					
						if (proceed) {
							PreviewUI* props = reinterpret_cast<PreviewUI*>(m_ui->Get(ViewID::Preview));
							props->Pick(item);
							m_data->Renderer.AddPickedItem(item);
						}
					}
				}
			}
		ImGui::Unindent(PIPELINE_ITEM_INDENT);
	}
}