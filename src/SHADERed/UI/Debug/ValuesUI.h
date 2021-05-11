#pragma once
#include <SHADERed/UI/UIView.h>
#include <SHADERed/Engine/Timer.h>

namespace ed {
	class DebugValuesUI : public UIView {
	public:
		using UIView::UIView;

		void Refresh();

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:
		std::unordered_map<std::string, std::string> m_cachedGlobals;
		std::unordered_map<std::string, std::string> m_cachedLocals;
	};
}