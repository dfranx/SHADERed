#pragma once
#include <SDL2/SDL.h>
#include "Plugin.h"
#include "../ShaderVariable.h"
#include <vector>
#include <string>

#define CURRENT_PLUGINAPI_VERSION 1

namespace ed
{
	class ObjectManager;

	class PluginManager
	{
	public:
		void Init(ObjectManager* objs); // load all the plugins here
		void Destroy(); // destroy all the plugins
		void Update(float delta);

		IPlugin* GetPlugin(const std::string& plugin);
		std::string GetPluginName(IPlugin* plugin);
		int GetPluginVersion(const std::string& plugin);

		inline const std::vector<std::string>& GetPluginList() { return m_names; }

		void ShowCustomMenu();
		void ShowMenuItems(const std::string& menu);
		void ShowContextItems(const std::string& menu);

		bool ShowSystemVariables(PluginSystemVariableData* data, ShaderVariable::ValueType type);
		bool ShowVariableFunctions(PluginFunctionData* data, ShaderVariable::ValueType type);

		void OnEvent(const SDL_Event& e);

		inline const std::vector<IPlugin*>& Plugins() { return m_plugins; }

	private:
		std::vector<void*> m_proc;
		std::vector<IPlugin*> m_plugins;
		std::vector<bool> m_isActive;
		std::vector<std::string> m_names;
		std::vector<int> m_versions;
	};
}