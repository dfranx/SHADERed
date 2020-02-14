#include "ImmediateUI.h"
#include "../UIHelper.h"
#include <imgui/imgui.h>

namespace ed
{
	void DebugImmediateUI::m_clear()
	{
		m_buffer.clear();
		m_lineOffsets.clear();
		m_lineOffsets.push_back(0);
	}
	void DebugImmediateUI::m_addLog(const std::string& str)
	{
		int old_size = m_buffer.size();
		m_buffer.append((str + "\n").c_str());
		for (int new_size = m_buffer.size(); old_size < new_size; old_size++)
			if (m_buffer[old_size] == '\n')
				m_lineOffsets.push_back(old_size + 1);
	}
	void DebugImmediateUI::OnEvent(const SDL_Event& e)
	{}
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
		while (clipper.Step())
		{
			for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
			{
				const char* line_start = buf + m_lineOffsets[line_no];
				const char* line_end = (line_no + 1 < m_lineOffsets.size()) ? (buf + m_lineOffsets[line_no + 1] - 1) : buf_end;
				ImGui::TextUnformatted(line_start, line_end);
			}
		}
		clipper.End();

		ImGui::PopStyleVar();

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();

		ImGui::PushItemWidth(-1);
		if (ImGui::InputText("##immediate_input", m_input, 512, ImGuiInputTextFlags_EnterReturnsTrue)) {
			m_addLog(std::string(m_input));
			m_addLog(UIHelper::GetVariableValue(m_data->Debugger.Engine.Immediate(m_input)));
			m_input[0] = 0;

			ImGui::SetKeyboardFocusHere(0);
		}
		ImGui::PopItemWidth();
	}
}