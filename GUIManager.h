#pragma once
#include <MoonLight/Base/Window.h>
#include "Objects/KeyboardShortcuts.h"
#include <map>

namespace ed
{
	class InterfaceManager;
	class CreateItemUI;
	class UIView;
	class Settings;

	class GUIManager
	{
	public:
		GUIManager(ed::InterfaceManager* objects, ml::Window* wnd);
		~GUIManager();

		void OnEvent(const ml::Event& e);
		void Update(float delta);
		void Render();

		UIView* Get(const std::string& name);

		void SaveSettings();
		void LoadSettings();

	private:
		void m_setupShortcuts();

		void m_imguiHandleEvent(const ml::Event& e);

		void m_renderOptions();
		bool m_optionsOpened;
		int m_optGroup;
		UIView* m_options;

		Settings* m_settingsBkp;
		std::map<std::string, KeyboardShortcuts::Shortcut> m_shortcutsBkp;

		bool m_savePreviewPopupOpened;
		std::string m_previewSavePath;
		DirectX::XMINT2 m_previewSaveSize;

		std::vector<UIView*> m_views;
		CreateItemUI* m_createUI;

		ml::Window* m_wnd;
		InterfaceManager* m_data;
		bool m_isCreateItemPopupOpened, m_isCreateRTOpened;

		void m_saveAsProject();
	};
}