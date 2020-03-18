#pragma once
#include <SHADERed/InterfaceManager.h>

namespace ed {
	class GUIManager;

	class UIView {
	public:
		UIView(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: m_ui(ui)
				, m_data(objects)
				, Visible(visible)
				, Name(name)
		{
		}
		virtual ~UIView() { }

		virtual void OnEvent(const SDL_Event& e) = 0;
		virtual void Update(float delta) = 0;

		bool Visible;
		std::string Name;

	protected:
		InterfaceManager* m_data;
		GUIManager* m_ui;
	};
}
