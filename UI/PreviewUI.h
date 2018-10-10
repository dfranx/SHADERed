#pragma once
#include "UIView.h"
#include "../ImGUI/imgui.h"

namespace ed
{
	class PreviewUI : public UIView
	{
	public:
		using UIView::UIView;

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

	private:
		ImVec2 m_mouseContact;
	};
}