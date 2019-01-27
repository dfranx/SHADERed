#pragma once
#include "UIView.h"
#include "../ImGUI/imgui.h"

namespace ed
{
	class ObjectListUI : public UIView
	{
	public:
		using UIView::UIView;

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

	};
}