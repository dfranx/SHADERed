#pragma once
#include <SHADERed/InterfaceManager.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class StatsPage : public UIView {
	public:
		StatsPage(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);
	};
}