#include <SHADERed/UI/Debug/AutoUI.h>
#include <SHADERed/Objects/Settings.h>
#include <imgui/imgui.h>

namespace ed {
	void DebugAutoUI::OnEvent(const SDL_Event& e)
	{
	}
	void DebugAutoUI::Update(float delta)
	{
		if (m_update) {
			m_value.resize(m_expr.size());

			for (int i = 0; i < m_expr.size(); i++) {
				spvm_result_t resType = nullptr;
				spvm_result_t exprVal = m_data->Debugger.Immediate(std::string(m_expr[i]), resType);

				if (exprVal != nullptr && resType != nullptr) {
					std::stringstream ss;
					m_data->Debugger.GetVariableValueAsString(ss, m_data->Debugger.GetVMImmediate(), resType, exprVal->members, exprVal->member_count, "");
					m_value[i] = ss.str();
				} else
					m_value[i] = "ERROR";
			}

			m_update = false;
		}

		// Main window
		ImGui::BeginChild("##auto_viewarea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		if (ImGui::BeginTable("##auto_tbl", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollFreezeTopRow | ImGuiTableFlags_ScrollY)) {
			ImGui::TableSetupColumn("Expression");
			ImGui::TableSetupColumn("Value");
			ImGui::TableAutoHeaders();

			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
			for (size_t i = 0; i < m_value.size(); i++) {
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%s", m_expr[i].c_str());

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%s", m_value[i].c_str());
			}
			ImGui::PopStyleColor();

			ImGui::EndTable();
		}

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
	}
}