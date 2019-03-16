#pragma once
#include "UIView.h"
#include "../Objects/KeyboardShortcuts.h"

namespace ed
{
	class OptionsUI : public UIView
	{
	public:
		enum class Page
		{
			General,
			Editor,
			Shortcuts,
			Preview
		};

		OptionsUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true) :
			UIView(ui, objects, name, visible),
			m_selectedShortcut(-1) { }
		//using UIView::UIView;

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

		inline bool IsListening() { return m_page == Page::Shortcuts && m_selectedShortcut != -1; }

		inline void SetGroup(Page grp) {
			m_page = grp;
			if (m_page ==Page::General)
				m_loadThemeList();
		}

		void ApplyTheme();

		inline std::vector<std::string> GetThemeList() { return m_themes; }

	private:
		Page m_page;

		int m_selectedShortcut;
		KeyboardShortcuts::Shortcut m_newShortcut;
		std::string m_getShortcutString();

		std::vector<std::string> m_themes;
		void m_loadThemeList();

		void m_renderGeneral();
		void m_renderEditor();
		void m_renderShortcuts();
		void m_renderPreview();
	};
}