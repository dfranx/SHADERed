#pragma once
#include <SDL2/SDL_events.h>
#include <SHADERed/Objects/DebugInformation.h>
#include <SHADERed/Objects/DebugAdapterProtocol.h>
#include <SHADERed/Objects/MessageStack.h>
#include <SHADERed/Objects/ObjectManager.h>
#include <SHADERed/Objects/PipelineManager.h>
#include <SHADERed/Objects/PluginManager.h>
#include <SHADERed/Objects/ProjectParser.h>
#include <SHADERed/Objects/RenderEngine.h>
#include <SHADERed/Objects/FrameAnalysis.h>
#include <SHADERed/Objects/WebAPI.h>

namespace ed {
	class GUIManager;

	class InterfaceManager {
	public:
		InterfaceManager(GUIManager* gui);
		~InterfaceManager();

		void OnEvent(const SDL_Event& e);
		void Update(float delta);

		void DebugClick(glm::vec2 r);
		void FetchPixel(PixelInformation& pixel);

		bool Run;

		PluginManager Plugins;
		RenderEngine Renderer;
		PipelineManager Pipeline;
		ObjectManager Objects;
		ProjectParser Parser;
		MessageStack Messages;
		DebugInformation Debugger;
		FrameAnalysis Analysis;
		WebAPI API;
		DebugAdapterProtocol DAP;

	private:
		GUIManager* m_ui;

		void m_fetchVertices(PixelInformation& pixel);
		bool m_canDebug();
	};
}