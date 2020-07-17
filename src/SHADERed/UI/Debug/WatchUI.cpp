#include <SHADERed/UI/Debug/WatchUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <imgui/imgui.h>

namespace ed {
	void DebugWatchUI::Refresh()
	{
		std::vector<char*>& exprs = m_data->Debugger.GetWatchList();
		for (size_t i = 0; i < exprs.size(); i++)
			m_data->Debugger.UpdateWatchValue(i);
	}
	void DebugWatchUI::OnEvent(const SDL_Event& e)
	{
	}
	void DebugWatchUI::Update(float delta)
	{
		std::vector<char*>& exprs = m_data->Debugger.GetWatchList();

		// Main window
		ImGui::BeginChild("##watch_viewarea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		ImGui::Separator();
		ImGui::Columns(2);

		ImGui::Text("Expression");
		ImGui::NextColumn();
		ImGui::Text("Value");
		ImGui::NextColumn();

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

		for (size_t i = 0; i < exprs.size(); i++) {
			ImGui::PushItemWidth(-1);
			if (ImGui::InputText(("##watch_expr_" + std::to_string(i)).c_str(), exprs[i], 512, ImGuiInputTextFlags_EnterReturnsTrue)) {
				if (strlen(exprs[i]) == 0) {
					m_data->Debugger.RemoveWatch(i);
					m_data->Parser.ModifyProject();
					i--;
					continue;
				} else {
					m_data->Debugger.UpdateWatchValue(i);
					m_data->Parser.ModifyProject();
				}
			}
			ImGui::PopItemWidth();
			ImGui::NextColumn();
			ImGui::Text(m_data->Debugger.GetWatchValue(i).c_str());
			ImGui::NextColumn();
			ImGui::Separator();
		}
		ImGui::PopStyleColor();

		ImGui::Columns();

		ImGui::PushItemWidth(-1);
		if (ImGui::InputText("##watch_new_expr", m_newExpr, 512, ImGuiInputTextFlags_EnterReturnsTrue)) {
			m_data->Debugger.AddWatch(m_newExpr);
			m_data->Parser.ModifyProject();
			m_newExpr[0] = 0;
		}
		ImGui::PopItemWidth();

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
	}
}