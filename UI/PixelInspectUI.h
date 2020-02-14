#pragma once
#include "UIView.h"

namespace ed
{
	class PixelInspectUI : public UIView
	{
	public:
		PixelInspectUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true) :
			UIView(ui, objects, name, visible) { }

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:

	};
}