#pragma once
#include <SHADERed/UI/UIView.h>
#include <imgui/imgui.h>

namespace ed {
	class DebugImmediateUI : public UIView {
	public:
		DebugImmediateUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
			m_input[0] = 0;
			m_clear();
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:
		void m_clear();
		void m_addLog(const std::string& str);

		char m_input[512];
		std::vector<int> m_lineOffsets;
		ImGuiTextBuffer m_buffer;
	};
}