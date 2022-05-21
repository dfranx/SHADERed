#pragma once
#include <SDL2/SDL.h>
#include <SHADERed/Objects/InputLayout.h>
#include <SHADERed/Objects/PluginAPI/Plugin.h>
#include <SHADERed/Objects/ShaderVariable.h>
#include <string>
#include <vector>

#define CURRENT_PLUGINAPI_VERSION 3

namespace ed {
	class InterfaceManager;
	class GUIManager;

	class PluginManager {
	public:
		void Init(InterfaceManager* data, GUIManager* ui); // load all the plugins here
		void Destroy();									   // destroy all the plugins
		void Update(float delta);

		void BeginRender();
		void EndRender();

		void RegisterPlugin(IPlugin1* plugin, const std::string pname, int apiVer, int pluginVer, void* procDLL = nullptr);
		
		IPlugin1* GetPlugin(const std::string& plugin);
		const std::string& GetPluginName(IPlugin1* plugin);
		int GetPluginVersion(const std::string& plugin);
		int GetPluginAPIVersion(const std::string& plugin);

		bool IsLoaded(const std::string& plugin);

		inline const std::vector<std::string>& GetPluginList() { return m_names; }

		void ShowCustomMenu();
		void ShowMenuItems(const std::string& menu);
		void ShowContextItems(const std::string& menu, void* owner = nullptr, void* extraData = nullptr);
		void ShowContextItems(IPlugin1* plugin, const std::string& menu, void* owner = nullptr);
		void ShowOptions(const std::string& searchString = "");

		void HandleDropFile(const char* filename);
		void HandleApplicationEvent(plugin::ApplicationEvent event, void* data1, void* data2);

		bool ShowSystemVariables(PluginSystemVariableData* data, ShaderVariable::ValueType type);
		bool ShowVariableFunctions(PluginFunctionData* data, ShaderVariable::ValueType type);

		std::vector<InputLayoutItem> BuildInputLayout(IPlugin1* plugin, const char* type, void* pldata);

		void OnEvent(const SDL_Event& e);

		inline const std::vector<IPlugin1*>& Plugins() { return m_plugins; }

		inline const std::vector<std::string>& GetIncompatiblePlugins() { return m_incompatible; }

	private:
		std::vector<void*> m_proc;
		std::vector<IPlugin1*> m_plugins;
		std::vector<std::string> m_names;
		std::vector<int> m_pluginVersion, m_apiVersion;

		
		std::vector<std::string> m_incompatible;

		InterfaceManager* m_data;
		GUIManager* m_ui;
		std::vector<std::string> m_iniLines;
	};
}