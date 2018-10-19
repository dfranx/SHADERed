#pragma once
#include <MoonLight/Base/Window.h>
#include <MoonLight/Base/Event.h>
#include "Objects/PipelineManager.h"
#include "Objects/RenderEngine.h"

namespace ed
{
	class InterfaceManager
	{
	public:
		InterfaceManager(ml::Window* wnd);

		void OnEvent(const ml::Event& e);
		void Update(float delta);

		inline ml::Window* GetOwner() { return m_wnd; }

		PipelineManager Pipeline;
		RenderEngine Renderer;

	private:
		ml::Window* m_wnd;
	};
}