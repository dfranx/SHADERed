#include "InterfaceManager.h"
#include "GUIManager.h"
#include "Objects/DefaultState.h"

namespace ed
{
	InterfaceManager::InterfaceManager(GUIManager* gui) :
		Renderer(&Pipeline, &Objects, &Parser, &Messages, &Plugins, &Debugger),
		Pipeline(&Parser),
		Objects(&Parser, &Renderer),
		Parser(&Pipeline, &Objects, &Renderer, &Plugins, &Messages, &Debugger, gui),
		Debugger(&Objects)
	{
		m_ui = gui;
	}
	InterfaceManager::~InterfaceManager()
	{
		Objects.Clear();
		Plugins.Destroy();
	}
	void InterfaceManager::OnEvent(const SDL_Event& e)
	{}
	void InterfaceManager::Update(float delta)
	{}
}