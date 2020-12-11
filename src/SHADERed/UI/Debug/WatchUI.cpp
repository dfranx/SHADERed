#include <SHADERed/UI/Debug/WatchUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <SHADERed/Objects/Settings.h>
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

		if (ImGui::BeginTable("##watches_tbl", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollFreezeTopRow | ImGuiTableFlags_ScrollY, ImVec2(0, Settings::Instance().CalculateSize(-ImGui::GetFontSize() - 10.0f)))) {
			ImGui::TableSetupColumn("Expression");
			ImGui::TableSetupColumn("Value");
			ImGui::TableAutoHeaders();

			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
			for (size_t i = 0; i < exprs.size(); i++) {
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::PushItemWidth(-1);
				ImGui::PushID(i);
				if (ImGui::InputText("##watch_expr", exprs[i], 512, ImGuiInputTextFlags_EnterReturnsTrue)) {
					if (strlen(exprs[i]) == 0) {
						m_data->Debugger.RemoveWatch(i);
						m_data->Parser.ModifyProject();
						i--;
						ImGui::PopID();
						ImGui::PopItemWidth();
						continue;
					} else {
						m_data->Debugger.UpdateWatchValue(i);
						m_data->Parser.ModifyProject();
					}
				}
				ImGui::PopID();
				ImGui::PopItemWidth();

				
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%s", m_data->Debugger.GetWatchValue(i).c_str());
			}
			ImGui::PopStyleColor();



			ImGui::EndTable();
		}

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