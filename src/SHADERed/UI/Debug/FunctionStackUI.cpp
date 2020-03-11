#include <SHADERed/UI/Debug/FunctionStackUI.h>
#include <imgui/imgui.h>

namespace ed {
	void DebugFunctionStackUI::Refresh()
	{
		m_stack = m_data->Debugger.Engine.GetFunctionStack();
	}
	void DebugFunctionStackUI::OnEvent(const SDL_Event& e)
	{
	}
	void DebugFunctionStackUI::Update(float delta)
	{
		for (int i = m_stack.size() - 1; i >= 0; i--)
			ImGui::Text(m_stack[i].c_str());
	}
}