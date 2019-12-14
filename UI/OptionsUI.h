#pragma once
#include "UIView.h"
#include "../Objects/Settings.h"
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
			Preview,
			Plugins,
			Project
		};

		OptionsUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true) :
			UIView(ui, objects, name, visible),
			m_selectedShortcut(-1), m_page(Page::General) {
			memset(m_shortcutSearch, 0, 256);
			memset(m_pluginSearch, 0, 256);
			m_pluginNotLoadedLB = 0;
			m_pluginLoadedLB = 0;
		}
		//using UIView::UIView;

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		inline bool IsListening() { return m_page == Page::Shortcuts && m_selectedShortcut != -1; }

		inline void SetGroup(Page grp) {
			m_page = grp;
			if (m_page == Page::General)
				m_loadThemeList();
			else if (m_page == Page::Preview) {
				switch (Settings::Instance().Preview.MSAA) {
				case 1: m_msaaChoice = 0; break;
				case 2: m_msaaChoice = 1; break;
				case 4: m_msaaChoice = 2; break;
				case 8: m_msaaChoice = 3; break;
				default: m_msaaChoice = 0; break;
				}
			}
			else if (m_page == Page::Plugins) {
				m_loadPluginList();
			}
		}
		Page GetGroup() { return m_page; }

		void ApplyTheme();

		inline std::vector<std::string> GetThemeList() { return m_themes; }

	private:
		Page m_page;

		char m_shortcutSearch[256];
		int m_selectedShortcut;
		KeyboardShortcuts::Shortcut m_newShortcut;
		std::string m_getShortcutString();

		int m_msaaChoice;

		char m_pluginSearch[256];
		std::vector<std::string> m_pluginsNotLoaded, m_pluginsLoaded;
		int m_pluginNotLoadedLB,
			m_pluginLoadedLB;
		void m_loadPluginList();

		std::vector<std::string> m_themes;
		void m_loadThemeList();

		void m_renderGeneral();
		void m_renderEditor();
		void m_renderShortcuts();
		void m_renderPreview();
		void m_renderPlugins();
		void m_renderProject();
	};
}