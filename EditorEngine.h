#pragma once
#include <MoonLight/Base/Event.h>
#include "InterfaceManager.h"
#include "GUIManager.h"

namespace ed
{
	class EditorEngine
	{
	public:
		EditorEngine(ml::Window* wnd);
		
		void OnEvent(const ml::Event& e);
		void Update(float delta);
		void Render();

		void Create();
		void Destroy();

	private:
		InterfaceManager m_interface;
		GUIManager m_ui;
	};
}