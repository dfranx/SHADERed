#include "InterfaceManager.h"
#include "GUIManager.h"

namespace ed
{
	InterfaceManager::InterfaceManager(GUIManager* gui, ml::Window* wnd) :
		Renderer(wnd, &Pipeline, &Parser, &Messages),
		Pipeline(wnd, &Parser),
		Parser(&Pipeline, gui)
	{
		m_ui = gui;
		m_wnd = wnd;
		Pipeline.New();
	}
	void InterfaceManager::OnEvent(const ml::Event & e)
	{}
	void InterfaceManager::Update(float delta)
	{}
}