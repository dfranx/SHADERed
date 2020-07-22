#include <SHADERed/UI/Debug/ImmediateUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

namespace ed {
	int HistoryCallback(ImGuiTextEditCallbackData* data)
	{
		HistoryData* history = (HistoryData*)data->UserData;

		if (history->Index >= (int)history->Data.size() || history->Data.size() == 0) {
			history->Index = -1;
			return 0;
		}

		if (data->EventKey == ImGuiKey_UpArrow) {
			history->Index++;
			if (history->Index >= history->Data.size())
				history->Index = 0;
		} else if (data->EventKey == ImGuiKey_DownArrow) {
			history->Index--;
			if (history->Index < 0)
				history->Index = 0;
		}

		data->BufDirty = true;
		strcpy(data->Buf, history->Data[history->Data.size() - history->Index - 1].c_str());
		data->BufTextLen = history->Data[history->Data.size() - history->Index - 1].size();
		data->CursorPos = data->BufTextLen;

		return 0;
	}

	void DebugImmediateUI::m_clear()
	{
		m_buffer.clear();
		m_lineOffsets.clear();
		m_isUserInput.clear();
		m_hasColorPreview.clear();
		m_colorPreview.clear();
		m_lineOffsets.push_back(0);
		m_history.Data.clear();
		m_history.Index = 0;
	}
	void DebugImmediateUI::m_addLog(const std::string& str, bool isUserInput, bool hasColorPreview, glm::vec4 color)
	{
		int old_size = m_buffer.size();
		m_buffer.append((str + "\n").c_str());

		for (int new_size = m_buffer.size(); old_size < new_size; old_size++)
			if (m_buffer[old_size] == '\n') {
				m_lineOffsets.push_back(old_size + 1);
				m_isUserInput.push_back(isUserInput);
				m_hasColorPreview.push_back(hasColorPreview);
				m_colorPreview.push_back(color);
			}
	}
	void DebugImmediateUI::OnEvent(const SDL_Event& e)
	{
	}
	void DebugImmediateUI::Update(float delta)
	{
		const float frame_size = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f + 5.0f;

		// Main window
		ImGui::BeginChild("##immediate_viewarea", ImVec2(0, -frame_size), false, ImGuiWindowFlags_HorizontalScrollbar);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		const char* buf = m_buffer.begin();
		const char* buf_end = m_buffer.end();

		ImGuiListClipper clipper;
		clipper.Begin(m_lineOffsets.size());
		while (clipper.Step()) {
			for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++) {
				const char* line_start = buf + m_lineOffsets[line_no];
				const char* line_end = (line_no + 1 < m_lineOffsets.size()) ? (buf + m_lineOffsets[line_no + 1] - 1) : buf_end;
				
				if (line_no < m_isUserInput.size() && m_isUserInput[line_no])
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.75f);

				if (line_no < m_hasColorPreview.size() && m_hasColorPreview[line_no]) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::ColorEdit4("##value_preview", const_cast<float*>(glm::value_ptr(m_colorPreview[line_no])), ImGuiColorEditFlags_NoInputs);
					ImGui::PopItemFlag();
					ImGui::SameLine();
				}

				ImGui::TextUnformatted(line_start, line_end);

				if (line_no < m_isUserInput.size() && m_isUserInput[line_no])
					ImGui::PopStyleVar();
			}
		}
		if (m_scrollRequired) {
			ImGui::SetScrollHereY();
			m_scrollRequired = false;
		}
		clipper.End();

		ImGui::PopStyleVar();

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();

		if (ImGui::BeginPopupContextItem("##context_immediate_dbg")) {
			if (ImGui::Selectable("Clear")) m_clear();
			ImGui::EndPopup();
		}

		ImGui::PushItemWidth(-1);
		if (ImGui::InputText("##immediate_input", m_input, 512, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory, HistoryCallback, (void*)&m_history)) {
			std::string inputStr(m_input);
			
			if (inputStr == "? clear") { // this is just a debug thingy, maybe support actual commands?
				m_clear();
			} else if (!inputStr.empty()) {
				
				spvm_result_t resType = nullptr;
				spvm_result_t res = m_data->Debugger.Immediate(inputStr, resType);

				m_scrollRequired = true;
				m_history.Add(inputStr);
				m_addLog(inputStr, true, false, glm::vec4(1.0f));

				if (res == nullptr || resType == nullptr) {
					m_addLog("ERROR", false, false, glm::vec4(1.0f));
				} else {
					std::stringstream ss;
					m_data->Debugger.GetVariableValueAsString(ss, m_data->Debugger.GetVMImmediate(), resType, res->members, res->member_count, "");
					
					bool hasColorPreview = false;
					glm::vec4 color(0.0f);
					if (resType->value_type == spvm_value_type_vector && m_data->Debugger.GetVMImmediate()->results[resType->pointer].value_type == spvm_value_type_float) {
						if (resType->member_count == 3)
							hasColorPreview = true;
						else if (resType->member_count == 4)
							hasColorPreview = true;

						for (int i = 0; i < resType->member_count; i++)
							color[i] = res->members[i].value.f;
					}

					std::string ssStr = ss.str();
					ssStr = ssStr.substr(0, ssStr.size() - 1);
					m_addLog(ssStr, false, hasColorPreview, color);
				}
			} else {
				m_addLog("\n", false, false, glm::vec4(0.0f));
			}

			m_input[0] = 0;
			ImGui::SetKeyboardFocusHere(0);
		}
		ImGui::PopItemWidth();
	}
}