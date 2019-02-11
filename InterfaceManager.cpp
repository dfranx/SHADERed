#include "InterfaceManager.h"
#include "GUIManager.h"
#include "Objects/DefaultState.h"

namespace ed
{
	InterfaceManager::InterfaceManager(GUIManager* gui, ml::Window* wnd) :
		Renderer(wnd, &Pipeline, &Objects, &Parser, &Messages),
		Pipeline(wnd, &Parser),
		Objects(wnd, &Parser, &Renderer),
		Parser(&Pipeline, &Objects, &Renderer, gui)
	{
		m_ui = gui;
		m_wnd = wnd;

		DefaultState::Instance().Create(*wnd);
	}
	void InterfaceManager::OnEvent(const ml::Event & e)
	{}
	void InterfaceManager::Update(float delta)
	{}
}