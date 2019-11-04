#pragma once
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

		inline const std::vector<IPlugin*>& Plugins() { return m_plugins; }

	private:
		std::vector<void*> m_proc;
		std::vector<IPlugin*> m_plugins;
		std::vector<bool> m_isActive;
		std::vector<std::string> m_names;
	};
}