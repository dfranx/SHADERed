#include "InterfaceManager.h"

namespace ed
{
	InterfaceManager::InterfaceManager(ml::Window* wnd) :
		Renderer(wnd, &Pipeline),
		Pipeline(wnd),
		Parser(&Pipeline)
	{
		m_wnd = wnd;
		Pipeline.New();
	}
	void InterfaceManager::OnEvent(const ml::Event & e)
	{}
	void InterfaceManager::Update(float delta)
	{}
}