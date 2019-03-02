#pragma once
#include "UIView.h"

namespace ed
{
	class OptionsUI : public UIView
	{
	public:
		enum class Page
		{
			General,
			Editor,
			Shortcuts,
			Preview
		};

		using UIView::UIView;

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

		inline void SetGroup(Page grp) {
			m_page = grp;
			if (m_page ==Page::General)
				m_loadThemeList();
		}

		void ApplyTheme();

	private:
		Page m_page;

		std::vector<std::string> m_themes;
		void m_loadThemeList();

		void m_renderGeneral();
		void m_renderEditor();
		void m_renderShortcuts();
		void m_renderPreview();
	};
}