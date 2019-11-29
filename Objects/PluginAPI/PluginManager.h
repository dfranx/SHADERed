#pragma once
#include <SDL2/SDL.h>
#include "Plugin.h"
#include <vector>
#include <string>

#define CURRENT_PLUGINAPI_VERSION 1

namespace ed
{
	class PluginManager
	{
	public:
		void Init(); // load all the plugins here
		void Destroy(); // destroy all the plugins
		void Update(float delta);

		void ShowCustomMenu();
		void ShowMenuItems(const std::string& menu);
		void ShowContextItems(const std::string& menu);

		void OnEvent(const SDL_Event& e);

		inline const std::vector<IPlugin*>& Plugins() { return m_plugins; }

	private:
		std::vector<void*> m_proc;
		std::vector<IPlugin*> m_plugins;
		std::vector<bool> m_isActive;
		std::vector<std::string> m_names;
	};
}