#include "InterfaceManager.h"
#include "GUIManager.h"
#include "Objects/DefaultState.h"

namespace ed
{
	InterfaceManager::InterfaceManager(GUIManager* gui, ml::Window* wnd) :
		Renderer(wnd, &Pipeline, &Parser, &Messages),
		Pipeline(wnd, &Parser),
		Parser(&Pipeline, &Renderer, gui)
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