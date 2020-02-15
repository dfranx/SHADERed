#include "ValuesUI.h"
#include "../UIHelper.h"
#include <imgui/imgui.h>

namespace ed
{
	void DebugValuesUI::OnEvent(const SDL_Event& e)
	{}
	void DebugValuesUI::Update(float delta)
	{
		sd::ShaderDebugger* dbgr = &m_data->Debugger.Engine;
		const auto& globals = dbgr->GetCompiler()->GetGlobals();
		const auto& locals = dbgr->GetCurrentFunctionLocals();
		const auto& funcs = dbgr->GetCompiler()->GetFunctions();
		std::string curFuncName = dbgr->GetCurrentFunction();
		sd::Function func;
		for (const auto& f : funcs) {
			if (f.Name == curFuncName) {
				func = f;
				break;
			}
		}
		const auto& args = func.Arguments;

		// Main window
		ImGui::BeginChild("##values_viewarea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		ImGui::Text("Globals:");
		ImGui::Separator();
		ImGui::Columns(2);
		for (const auto& global : globals) {
			bv_variable* val = dbgr->GetGlobalValue(global.Name);
			ImGui::Text(global.Name.c_str());
			ImGui::NextColumn();
			ImGui::Text(UIHelper::GetVariableValue(*val).c_str());
			ImGui::NextColumn();
			ImGui::Separator();
		}
		ImGui::Columns();

		if (args.size() > 0) {
			ImGui::NewLine();

			ImGui::Text("Arguments:");
			ImGui::Separator();
			ImGui::Columns(2);
			for (const auto& arg : args) {
				bv_variable* val = dbgr->GetLocalValue(arg.Name);
				if (val == nullptr)
					continue;
				ImGui::Text(arg.Name.c_str());
				ImGui::NextColumn();
				ImGui::Text(UIHelper::GetVariableValue(*val).c_str());
				ImGui::NextColumn();
				ImGui::Separator();
			}
			ImGui::Columns();
		}

		ImGui::NewLine();

		ImGui::Text("Locals:");
		ImGui::Separator();
		ImGui::Columns(2);
		for (const auto& local : locals) {
			bv_variable* val = dbgr->GetLocalValue(local);
			if (val == nullptr)
				continue;
			ImGui::Text(local.c_str());
			ImGui::NextColumn();
			ImGui::Text(UIHelper::GetVariableValue(*val).c_str());
			ImGui::NextColumn();
			ImGui::Separator();
		}
		ImGui::Columns();

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
	}
}