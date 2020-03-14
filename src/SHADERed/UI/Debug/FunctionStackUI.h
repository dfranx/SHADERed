#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	class DebugFunctionStackUI : public UIView {
	public:
		using UIView::UIView;

		void Refresh();

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:
		std::vector<std::string> m_stack;
	};
}