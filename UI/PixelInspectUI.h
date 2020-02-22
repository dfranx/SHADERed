#pragma once
#include "UIView.h"

namespace ed
{
	class PixelInspectUI : public UIView
	{
	public:
		PixelInspectUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true) :
			UIView(ui, objects, name, visible) {
			m_errorPopup = false;
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:
		bool m_errorPopup;
		std::string m_errorMessage;
	};
}