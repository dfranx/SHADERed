#include "WatchUI.h"
#include "../UIHelper.h"
#include <imgui/imgui.h>

namespace ed
{
	void DebugWatchUI::Add(const char* expr)
	{
		m_expr.push_back(new char[512]);
		memcpy(m_expr[m_expr.size() - 1], expr, strlen(expr)+1);

		if (m_data->Debugger.IsDebugging()) {
			bv_variable exprVal = m_data->Debugger.Engine.Immediate(expr);
			m_values.push_back(UIHelper::GetVariableValue(exprVal));
			bv_variable_deinitialize(&exprVal);
		}
	}
	void DebugWatchUI::Clear()
	{
		for (int i = 0; i < m_expr.size(); i++)
			delete[] m_expr[i];
		m_expr.clear();
		m_values.clear();
	}
	void DebugWatchUI::Refresh()
	{
		for (int i = 0; i < m_expr.size(); i++) {
			bv_variable exprVal = m_data->Debugger.Engine.Immediate(m_expr[i]);

			m_values[i] = UIHelper::GetVariableValue(exprVal);

			bv_variable_deinitialize(&exprVal);
		}
	}
	void DebugWatchUI::OnEvent(const SDL_Event& e)
	{}
	void DebugWatchUI::Update(float delta)
	{
		sd::ShaderDebugger* dbgr = &m_data->Debugger.Engine;

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

		for (int i = 0; i < m_expr.size(); i++) {
			if (ImGui::InputText(("##watch_expr_" + std::to_string(i)).c_str(), m_expr[i], 512, ImGuiInputTextFlags_EnterReturnsTrue)) {
				if (strlen(m_expr[i]) == 0) {
					delete[] m_expr[i];
					m_expr.erase(m_expr.begin() + i);
					m_values.erase(m_values.begin() + i);
					i--;
					continue;
				}
				else {
					bv_variable exprVal = m_data->Debugger.Engine.Immediate(m_expr[i]);
					m_values[i] = UIHelper::GetVariableValue(exprVal);
					bv_variable_deinitialize(&exprVal);
				}
			}
			ImGui::NextColumn();
			ImGui::Text(m_values[i].c_str());
			ImGui::NextColumn();
			ImGui::Separator();
		}
		ImGui::PopStyleColor();

		ImGui::Columns();

		ImGui::PushItemWidth(-1);
		if (ImGui::InputText("##watch_new_expr", m_newExpr, 512, ImGuiInputTextFlags_EnterReturnsTrue)) {
			Add(m_newExpr);
			m_newExpr[0] = 0;
		}
		ImGui::PopItemWidth();


		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
	}
}