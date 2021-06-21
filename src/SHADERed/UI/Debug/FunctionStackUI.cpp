#include <SHADERed/UI/Debug/FunctionStackUI.h>
#include <imgui/imgui.h>

namespace ed {
	void DebugFunctionStackUI::Refresh()
	{
		m_stack.clear();
		spvm_state_t vm = m_data->Debugger.GetVM();
		
		if (vm && vm->function_stack_info == nullptr)
			return;

		const std::vector<int>& lines = m_data->Debugger.GetFunctionStackLines();

		for (int i = vm->function_stack_current; i >= 0; i--) {
			spvm_result_t func = vm->function_stack_info[i];
			if (!(vm->owner->language == SpvSourceLanguageHLSL && i == 0) && func->name != nullptr) {
				std::string fname(func->name);
				if (fname.size() > 0 && fname[0] == '@') // clean up the @main(
					fname = fname.substr(1);
				size_t parenth = fname.find('(');
				if (parenth != std::string::npos)
					fname = fname.substr(0, parenth);

				int line = 0;
				if (i >= lines.size()) line = vm->current_line;
				else line = lines[i];

				if (vm->owner->language == SpvSourceLanguageHLSL)
					line--;

				m_stack.push_back(fname + " @ line " + std::to_string(line));
			}
		}
	}
	void DebugFunctionStackUI::OnEvent(const SDL_Event& e)
	{
	}
	void DebugFunctionStackUI::Update(float delta)
	{
		for (int i = 0; i < m_stack.size(); i++)
			ImGui::Text("%s", m_stack[i].c_str());
	}
}