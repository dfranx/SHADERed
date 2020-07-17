#pragma once
#include <SHADERed/UI/UIView.h>
#include <imgui/imgui.h>
#include <functional>

namespace ed {
	struct HistoryData {
		std::vector<std::string> Data;
		int Index;

		void Add(const std::string& entry)
		{
			Data.push_back(entry);
			Index = -1;

			if (Data.size() > 30)
				Data.erase(Data.begin());
		}
	};

	class DebugImmediateUI : public UIView {
	public:
		DebugImmediateUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
			m_input[0] = 0;
			m_scrollRequired = false;
			m_clear();
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:
		void m_clear();
		void m_addLog(const std::string& str, bool isUserInput, bool hasColorPreview, glm::vec4 color);

		HistoryData m_history;

		bool m_scrollRequired;

		char m_input[512];
		std::vector<int> m_lineOffsets;
		std::vector<bool> m_isUserInput, m_hasColorPreview;
		std::vector<glm::vec4> m_colorPreview;
		ImGuiTextBuffer m_buffer;
	};
}