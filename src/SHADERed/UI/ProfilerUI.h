#pragma once
#include <SDL2/SDL_events.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class ProfilerUI : public UIView {
	public:
		ProfilerUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
		}
		~ProfilerUI() { }

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:
		void m_renderRow(int index, const char* name, uint64_t time, uint64_t timeOffset, uint64_t totalTime);
	};
}