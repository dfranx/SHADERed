#pragma once
#include "UIView.h"
#include "Tools/CubemapPreview.h"

namespace ed
{
	class PixelInspectUI : public UIView
	{
	public:
		PixelInspectUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true) :
			UIView(ui, objects, name, visible) {
			m_errorPopup = false;
			m_cubePrev.Init(152, 114);
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:
		bool m_errorPopup;
		std::string m_errorMessage;
		CubemapPreview m_cubePrev;
		
	};
}