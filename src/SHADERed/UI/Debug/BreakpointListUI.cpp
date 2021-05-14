#include <SHADERed/UI/Debug/BreakpointListUI.h>
#include <ImGuiColorTextEdit/TextEditor.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <imgui/imgui.h>
#include <filesystem>

namespace ed {
	void DebugBreakpointListUI::OnEvent(const SDL_Event& e)
	{
	}
	void DebugBreakpointListUI::Update(float delta)
	{
		const auto& bkpts = m_data->Debugger.GetBreakpointList();
		auto bkptStates = m_data->Debugger.GetBreakpointStateList();
		bool isEnabled = false;

		if (ImGui::BeginTable("##breakpoints_tbl", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollFreezeTopRow | ImGuiTableFlags_ScrollY, ImVec2(0, Settings::Instance().CalculateSize(-ImGui::GetFontSize() - 10.0f)))) {
			ImGui::TableSetupColumn("File");
			ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
			ImGui::TableSetupColumn("Condition");
			ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
			ImGui::TableAutoHeaders();

			for (const auto& bkpt : bkpts) {
				std::string fileNameStr = std::filesystem::path(bkpt.first).filename().string();
				const char* fileName = fileNameStr.c_str();
				for (int i = 0; i < bkpt.second.size(); i++) {
					ImGui::PushID(i);


					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", fileName);

					ImGui::TableSetColumnIndex(1);
					const dbg::Breakpoint& b = bkpt.second[i];
					isEnabled = bkptStates[bkpt.first][i];
					if (ImGui::Checkbox("##bkpt_state", &isEnabled)) {
						m_data->Debugger.SetBreakpointEnabled(std::filesystem::absolute(bkpt.first).generic_u8string(), b.Line, isEnabled);
						TextEditor* textEd = ((CodeEditorUI*)m_ui->Get(ViewID::Code))->Get(std::filesystem::absolute(bkpt.first).generic_u8string());
						if (textEd != nullptr)
							textEd->SetBreakpointEnabled(b.Line, isEnabled);
					}

					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%s", b.Condition.c_str());

					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%d", b.Line);


					ImGui::PopID();
				}
			}

			ImGui::EndTable();
		}
	}
}