#pragma once
#include "InterfaceManager.h"

namespace ed
{
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

		bool m_applySize;

		ml::Window* m_wnd;
		InterfaceManager* m_data;
	};
}