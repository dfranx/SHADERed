#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	class MessageOutputUI : public UIView {
	public:
		using UIView::UIView;

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);
	};
}