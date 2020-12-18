#pragma once
#include <SHADERed/UI/UIView.h>
#include <string>
#include <vector>

namespace ed {
	class DebugAutoUI : public UIView {
	public:
		DebugAutoUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{ 
			m_update = false;
		}

		inline void SetExpressions(const std::vector<std::string>& exprs) { 
			m_expr = exprs;
			m_value.clear();
			m_update = true;
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:
		std::vector<std::string> m_value;
		std::vector<std::string> m_expr;
		bool m_update;
	};
}