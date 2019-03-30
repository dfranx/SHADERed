#pragma once
#include "UIView.h"

namespace ed
{
	class MessageOutputUI : public UIView
	{
	public:
		using UIView::UIView;

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);
	};
}