#include "../Objects/PluginAPI/PluginManager.h" // TODO: reorganize SHADERed source code & add_include_dir(inc)
#include <imgui/imgui.h>
#include <ghc/filesystem.hpp>

namespace ed
{
	typedef IPlugin* (*CreatePluginFn)(ImGuiContext* ctx);
	typedef void (*DestroyPluginFn)(IPlugin* plugin);
	typedef int (*GetPluginAPIVersionFn)();
	typedef const char* (*GetPluginNameFn)();

	void PluginManager::OnEvent(const SDL_Event& e)
	{
		for (const auto& plugin : m_plugins)
			plugin->OnEvent((void*)&e);
	}
	void PluginManager::Update(float delta)
	{
		for (const auto& plugin : m_plugins)
			plugin->Update(delta);
	}
	void PluginManager::ShowContextItems(const std::string& menu)
	{
		for (const auto& plugin : m_plugins) {
			if (plugin->HasContextItems(menu.c_str())) {
				ImGui::Separator();
				plugin->ShowContextItems(menu.c_str());
			}
		}
	}
	void PluginManager::ShowMenuItems(const std::string& menu)
	{
		for (const auto& plugin : m_plugins) {
			if (plugin->HasMenuItems(menu.c_str())) {
				ImGui::Separator();
				plugin->ShowMenuItems(menu.c_str());
			}
		}
	}
	void PluginManager::ShowCustomMenu()
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_plugins[i]->HasCustomMenu())
				if (ImGui::BeginMenu(m_names[i].c_str())) {
					m_plugins[i]->ShowMenuItems("custom");
					ImGui::EndMenu();
				}
	}
	void PluginManager::Init()
	{
		ImGuiContext* uiCtx = ImGui::GetCurrentContext();

		// TODO: linux & error messages
		for (const auto& entry : ghc::filesystem::directory_iterator("./plugins/")) {
			if (entry.is_directory()) {
				std::string pdir = entry.path().filename().native();

				HINSTANCE procDLL = LoadLibraryA(std::string("./plugins/" + pdir + "/plugin.dll").c_str());

				if (!procDLL)
					continue;

				// GetPluginAPIVersion() function
				GetPluginAPIVersionFn fnGetPluginAPIVersion = (GetPluginAPIVersionFn)GetProcAddress(procDLL, "GetPluginAPIVersion");
				if (!fnGetPluginAPIVersion) {
					FreeLibrary(procDLL);
					continue;
				}

				int pver = (*fnGetPluginAPIVersion)();
				if (pver != CURRENT_PLUGINAPI_VERSION) {
					FreeLibrary(procDLL);
					continue;
				}

				// CreatePlugin() function
				CreatePluginFn fnCreatePlugin = (CreatePluginFn)GetProcAddress(procDLL, "CreatePlugin");
				if (!fnCreatePlugin) {
					FreeLibrary(procDLL);
					continue;
				}

				// GetPluginName() function
				GetPluginNameFn fnGetPluginName = (GetPluginNameFn)GetProcAddress(procDLL, "GetPluginName");
				if (!fnGetPluginName) {
					FreeLibrary(procDLL);
					continue;
				}

				// create the actual plugin
				IPlugin* plugin = (*fnCreatePlugin)(uiCtx);
				if (plugin == nullptr) {
					FreeLibrary(procDLL);
					continue;
				}

				printf("[DEBUG] Loaded plugin %s\n", pdir.c_str());

				// now we can add the plugin and the proc to the list, init the plugin, etc...
				plugin->Init();
				m_plugins.push_back(plugin);
				m_proc.push_back(procDLL);
				m_isActive.push_back(true);
				m_names.push_back((*fnGetPluginName)());
			}
		}
	}
	void PluginManager::Destroy()
	{
		for (int i = 0; i < m_plugins.size(); i++) {
			m_plugins[i]->Destroy();

			DestroyPluginFn fnDestroyPlugin = (DestroyPluginFn)GetProcAddress((HINSTANCE)m_proc[i], "DestroyPlugin");
			if (fnDestroyPlugin)
				(*fnDestroyPlugin)(m_plugins[i]);

			FreeLibrary((HINSTANCE)m_proc[i]);
		}

		m_plugins.clear();
	}
}