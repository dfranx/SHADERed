#pragma once
#include <SHADERed/Objects/DebugInformation.h>
#include <SHADERed/Objects/MessageStack.h>
#include <SHADERed/Objects/ObjectManager.h>
#include <SHADERed/Objects/PipelineManager.h>
#include <SHADERed/Objects/PluginAPI/PluginManager.h>
#include <SHADERed/Objects/ProjectParser.h>
#include <SHADERed/Objects/RenderEngine.h>
#include <SDL2/SDL_events.h>

namespace ed {
	class GUIManager;

	class InterfaceManager {
	public:
		InterfaceManager(GUIManager* gui);
		~InterfaceManager();

		void OnEvent(const SDL_Event& e);
		void Update(float delta);

		bool FetchPixel(PixelInformation& pixel);

		PluginManager Plugins;
		RenderEngine Renderer;
		PipelineManager Pipeline;
		ObjectManager Objects;
		ProjectParser Parser;
		MessageStack Messages;
		DebugInformation Debugger;

	private:
		GUIManager* m_ui;
	};
}