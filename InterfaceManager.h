#pragma once
#include <MoonLight/Base/Window.h>
#include <MoonLight/Base/Event.h>
#include "Objects/PipelineManager.h"
#include "Objects/ObjectManager.h"
#include "Objects/ProjectParser.h"
#include "Objects/RenderEngine.h"
#include "Objects/MessageStack.h"

namespace ed
{
	class GUIManager;

	class InterfaceManager
	{
	public:
		InterfaceManager(GUIManager* gui, ml::Window* wnd);

		void OnEvent(const ml::Event& e);
		void Update(float delta);

		inline ml::Window* GetOwner() { return m_wnd; }

		PipelineManager Pipeline;
		RenderEngine Renderer;
		ProjectParser Parser;
		MessageStack Messages;
		ObjectManager Objects;

	private:
		GUIManager* m_ui;
		ml::Window* m_wnd;
	};
}