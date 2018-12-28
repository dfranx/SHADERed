#pragma once
#include <MoonLight/Base/Window.h>

namespace ed
{
	class InterfaceManager;
	class CreateItemUI;
	class UIView;

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
		void m_imguiHandleEvent(const ml::Event& e);

		std::vector<UIView*> m_views;
		CreateItemUI* m_createUI;

		ml::Window* m_wnd;
		InterfaceManager* m_data;

		void m_openProject();
		void m_saveAsProject();
	};
}