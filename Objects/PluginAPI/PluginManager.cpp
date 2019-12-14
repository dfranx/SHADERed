#include "PluginManager.h"
#include "../Settings.h"
#include "../SystemVariableManager.h"
#include "../../InterfaceManager.h"
#include "../../GUIManager.h"
#include "../../UI/PreviewUI.h"

#include <algorithm>
#include <imgui/imgui.h>
#include <ghc/filesystem.hpp>

#if defined(__linux__) || defined(__unix__)
	#include <dlfcn.h>
#endif

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
	void PluginManager::Init(InterfaceManager* data)
	{
		ImGuiContext* uiCtx = ImGui::GetCurrentContext();

		// TODO: linux & error messages
		for (const auto& entry : ghc::filesystem::directory_iterator("./plugins/")) {
			if (entry.is_directory()) {
				std::string pdir = entry.path().filename().native();

#if defined(__linux__) || defined(__unix__)
				void* procDLL = dlopen(("./plugins/" + pdir + "/plugin.so").c_str(), RTLD_NOW);

				if (!procDLL)
					continue;

				// GetPluginAPIVersion() function
				GetPluginAPIVersionFn fnGetPluginAPIVersion = (GetPluginAPIVersionFn)dlsym(procDLL, "GetPluginAPIVersion");
				if (!fnGetPluginAPIVersion) {
					dlclose(procDLL);
					continue;
				}

				int pver = (*fnGetPluginAPIVersion)();
				m_versions.push_back(pver);

				// CreatePlugin() function
				CreatePluginFn fnCreatePlugin = (CreatePluginFn)dlsym(procDLL, "CreatePlugin");
				if (!fnCreatePlugin) {
					dlclose(procDLL);
					continue;
				}

				// GetPluginName() function
				GetPluginNameFn fnGetPluginName = (GetPluginNameFn)dlsym(procDLL, "GetPluginName");
				if (!fnGetPluginName) {
					dlclose(procDLL);
					continue;
				}

				// create the actual plugin
				IPlugin* plugin = (*fnCreatePlugin)(uiCtx);
				if (plugin == nullptr) {
					dlclose(procDLL);
					continue;
				}
#else
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
				m_versions.push_back(pver);

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
#endif

				printf("[DEBUG] Loaded plugin %s\n", pdir.c_str());

				// list of loaded plugins
				std::vector<std::string> notLoaded = Settings::Instance().Plugins.NotLoaded;
				std::string pname = (*fnGetPluginName)();

				// set up pointers to app functions
				plugin->ObjectManager = (void*)&data->Objects;
				plugin->PipelineManager = (void*)&data->Pipeline;
				plugin->Renderer = (void*)&data->Renderer;
				plugin->Messages = (void*)&data->Messages;
				plugin->Project = (void*)&data->Parser;

				plugin->AddObject = [](void* objectManager, const char* name, const char* type, void* data, unsigned int id, void* owner) {
					ObjectManager* objm = (ObjectManager*)objectManager;
					objm->CreatePluginItem(name, type, data, id, (IPlugin*)owner);
				};
				plugin->AddCustomPipelineItem = [](void* pipeManager, void* parentPtr, const char* name, const char* type, void* data, void* owner) -> bool {
					PipelineManager* pipe = (PipelineManager*)pipeManager;
					PipelineItem* parent = (PipelineItem*)parentPtr;
					char* parentName = nullptr;

					if (parent != nullptr)
						parentName = parent->Name;

					return pipe->AddPluginItem(parentName, name, type, data, (IPlugin*)owner);
				};
				plugin->AddMessage = [](void* messages, plugin::MessageType mtype, const char* group, const char* txt) {
					MessageStack* msgs = (MessageStack*)messages;
					msgs->Add((MessageStack::Type)mtype, group, txt);
				};
				plugin->CreateRenderTexture = [](void* objects, const char* name) -> bool {
					ObjectManager* objs = (ObjectManager*)objects;
					return objs->CreateRenderTexture(name);
				};
				plugin->CreateImage = [](void* objects, const char* name, int width, int height) -> bool {
					ObjectManager* objs = (ObjectManager*)objects;
					return objs->CreateImage(name, glm::ivec2(width, height));
				};
				plugin->ResizeRenderTexture = [](void* objects, const char* name, int width, int height) {
					ObjectManager* objs = (ObjectManager*)objects;
					objs->ResizeRenderTexture(name, glm::ivec2(width, height));
				};
				plugin->ResizeImage = [](void* objects, const char* name, int width, int height) {
					ObjectManager* objs = (ObjectManager*)objects;
					objs->ResizeImage(name, glm::ivec2(width, height));
				};
				plugin->ExistsObject = [](void* objects, const char* name) -> bool {
					ObjectManager* objs = (ObjectManager*)objects;
					return objs->Exists(name);
				};
				plugin->RemoveGlobalObject = [](void* objects, const char* name) {
					ObjectManager* objs = (ObjectManager*)objects;
					objs->Remove(name);
				};
				plugin->GetProjectPath = [](void* project, const char* filename, char* out) {
					ProjectParser* proj = (ProjectParser*)project;
					std::string path = proj->GetProjectPath(filename);
					strcpy(out, path.c_str());
				};
				plugin->GetRelativePath = [](void* project, const char* filename, char* out) {
					ProjectParser* proj = (ProjectParser*)project;
					std::string path = proj->GetRelativePath(filename);
					strcpy(out, path.c_str());
				};
				plugin->GetProjectFilename = [](void* project, char* out) {
					ProjectParser* proj = (ProjectParser*)project;
					std::string path = proj->GetOpenedFile();
					strcpy(out, path.c_str());
				};
				plugin->GetProjectDirectory = [](void* project, char* out) {
					ProjectParser* proj = (ProjectParser*)project;
					std::string path = proj->GetProjectDirectory();
					strcpy(out, path.c_str());
				};
				plugin->IsProjectModified = [](void* project) -> bool {
					ProjectParser* proj = (ProjectParser*)project;
					return proj->IsProjectModified();
				};
				plugin->ModifyProject = [](void* project) {
					ProjectParser* proj = (ProjectParser*)project;
					proj->ModifyProject();
				};
				plugin->OpenProject = [](void* project, const char* filename) {
					ProjectParser* proj = (ProjectParser*)project;
					proj->Open(filename);
				};
				plugin->SaveProject = [](void* project) {
					ProjectParser* proj = (ProjectParser*)project;
					proj->Save();
				};
				plugin->SaveAsProject = [](void* project, const char* filename, bool copyFiles) {
					ProjectParser* proj = (ProjectParser*)project;
					proj->SaveAs(filename, copyFiles);
				};
				plugin->IsPaused = [](void* renderer) -> bool {
					RenderEngine* rend = (RenderEngine*)renderer;
					return rend->IsPaused();
				};
				plugin->Pause = [](void* renderer, bool state) {
					RenderEngine* rend = (RenderEngine*)renderer;
					rend->Pause(state);
				};
				plugin->GetRenderTexture = [](void* renderer, unsigned int& out) {
					RenderEngine* rend = (RenderEngine*)renderer;
					out = rend->GetTexture();
				};
				plugin->GetLastRenderSize = [](void* renderer, int& w, int& h) {
					RenderEngine* rend = (RenderEngine*)renderer;
					glm::ivec2 sz = rend->GetLastRenderSize();
					w = sz.x;
					h = sz.y;
				};
				plugin->Render = [](void* renderer, int w, int h) {
					RenderEngine* rend = (RenderEngine*)renderer;
					rend->Render(w, h);
				};
				plugin->ExistsPipelineItem = [](void* pipeline, const char* name) -> bool {
					PipelineManager* pipe = (PipelineManager*)pipeline;
					return pipe->Has(name);
				};
				plugin->GetPipelineItem = [](void* pipeline, const char* name) -> void* {
					PipelineManager* pipe = (PipelineManager*)pipeline;
					return (void*)pipe->Get(name);
				};
				plugin->BindShaderPassVariables = [](void* shaderpass, void* item) {
					pipe::ShaderPass* data = (pipe::ShaderPass*)shaderpass;
					data->Variables.Bind(item);
				};
				plugin->GetViewMatrix = [](float* out) {
					glm::mat4 viewm = SystemVariableManager::Instance().GetViewMatrix();
					memcpy(out, glm::value_ptr(viewm), sizeof(float) * 4 * 4);
				};
				plugin->GetProjectionMatrix = [](float* out) {
					glm::mat4 projm = SystemVariableManager::Instance().GetProjectionMatrix();
					memcpy(out, glm::value_ptr(projm), sizeof(float) * 4 * 4);
				};
				plugin->GetOrthographicMatrix = [](float* out) {
					glm::mat4 orthom = SystemVariableManager::Instance().GetOrthographicMatrix();
					memcpy(out, glm::value_ptr(orthom), sizeof(float) * 4 * 4);
				};
				plugin->GetViewportSize = [](float& w, float& h) {
					glm::vec2 viewsize = SystemVariableManager::Instance().GetViewportSize();
					w = viewsize.x;
					h = viewsize.y;
				};
				plugin->AdvanceTimer = [](float t) {
					SystemVariableManager::Instance().AdvanceTimer(t);
				};
				plugin->GetMousePosition = [](float& x, float& y) {
					glm::vec2 mpos = SystemVariableManager::Instance().GetMousePosition();
					x = mpos.x;
					y = mpos.y;
				};
				plugin->GetFrameIndex = []() -> int {
					return SystemVariableManager::Instance().GetFrameIndex();
				};
				plugin->GetTime = []() -> float {
					return SystemVariableManager::Instance().GetTime();
				};
				plugin->SetGeometryTransform = [](void* item, float scale[3], float rota[3], float pos[3]) {
					SystemVariableManager::Instance().SetGeometryTransform((PipelineItem*)item, glm::make_vec3(scale), glm::make_vec3(rota), glm::make_vec3(pos));
				};
				plugin->SetMousePosition = [](float x, float y) {
					SystemVariableManager::Instance().SetMousePosition(x,y);
				};
				plugin->SetKeysWASD = [](bool w, bool a, bool s, bool d) {
					SystemVariableManager::Instance().SetKeysWASD(w,a,s,d);
				};
				plugin->SetFrameIndex = [](int findex) {
					SystemVariableManager::Instance().SetFrameIndex(findex);
				};

				// now we can add the plugin and the proc to the list, init the plugin, etc...
				plugin->Init();
				m_plugins.push_back(plugin);
				m_proc.push_back(procDLL);
				m_isActive.push_back(std::count(notLoaded.begin(), notLoaded.end(), pname) == 0);
				m_names.push_back(pname);
			}
		}
	}
	void PluginManager::Destroy()
	{
		for (int i = 0; i < m_plugins.size(); i++) {
			m_plugins[i]->Destroy();
			#if defined(__linux__) || defined(__unix__)
				DestroyPluginFn fnDestroyPlugin = (DestroyPluginFn)dlsym(m_proc[i], "DestroyPlugin");
				if (fnDestroyPlugin)
					(*fnDestroyPlugin)(m_plugins[i]);

				dlclose(m_proc[i]);
			#else
				DestroyPluginFn fnDestroyPlugin = (DestroyPluginFn)GetProcAddress((HINSTANCE)m_proc[i], "DestroyPlugin");
				if (fnDestroyPlugin)
					(*fnDestroyPlugin)(m_plugins[i]);

				FreeLibrary((HINSTANCE)m_proc[i]);
			#endif
		}

		m_plugins.clear();
	}
	void PluginManager::Update(float delta)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_isActive[i])
				m_plugins[i]->Update(delta);
	}

	std::vector<InputLayoutItem> PluginManager::BuildInputLayout(IPlugin* plugin, const char* itemName)
	{
		int size = plugin->GetPipelineItemInputLayoutSize(itemName);
		std::vector<InputLayoutItem> inpLayout;
		for (int j = 0; j < size; j++) {
			plugin::InputLayoutItem inpOut;
			plugin->GetPipelineItemInputLayoutItem(itemName, j, inpOut);

			InputLayoutItem inp;
			inp.Value = (InputLayoutValue)inpOut.Value;
			inp.Semantic = std::string(inpOut.Semantic);

			inpLayout.push_back(inp);
		}

		return inpLayout;
	}

	IPlugin* PluginManager::GetPlugin(const std::string& plugin)
	{
		for (int i = 0; i < m_names.size(); i++)
			if (m_names[i] == plugin)
				return m_plugins[i];

		return nullptr;
	}
	std::string PluginManager::GetPluginName(IPlugin* plugin)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_plugins[i] == plugin)
				return m_names[i];

		return "";
	}
	int PluginManager::GetPluginVersion(const std::string& plugin)
	{
		for (int i = 0; i < m_names.size(); i++)
			if (m_names[i] == plugin)
				return m_versions[i];

		return 0;
	}

	void PluginManager::HandleDropFile(const char* filename)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_isActive[i] && m_plugins[i]->HandleDropFile(filename))
				break;
	}

	void PluginManager::ShowContextItems(const std::string& menu, void* owner)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_isActive[i] && m_plugins[i]->HasContextItems(menu.c_str())) {
				ImGui::Separator();
				m_plugins[i]->ShowContextItems(menu.c_str(), owner);
			}
	}
	void PluginManager::ShowContextItems(IPlugin* plugin, const std::string& menu, void* owner)
	{
		if (plugin->HasContextItems(menu.c_str())) {
			ImGui::Separator();
			plugin->ShowContextItems(menu.c_str(), owner);
		}
	}
	void PluginManager::ShowMenuItems(const std::string& menu)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_isActive[i] && m_plugins[i]->HasMenuItems(menu.c_str())) {
				ImGui::Separator();
				m_plugins[i]->ShowMenuItems(menu.c_str());
			}
	}
	void PluginManager::ShowCustomMenu()
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_isActive[i] && m_plugins[i]->HasCustomMenu())
				if (ImGui::BeginMenu(m_names[i].c_str())) {
					m_plugins[i]->ShowMenuItems("custom");
					ImGui::EndMenu();
				}
	}
	void PluginManager::ShowOptions(const std::string& searchString)
	{
		std::string qLower = searchString;
		std::transform(qLower.begin(), qLower.end(), qLower.begin(), tolower);

		for (int i = 0; i < m_plugins.size(); i++) {
			if (m_isActive[i] && m_plugins[i]->HasSectionInOptions()) {
				std::string pluginName = m_names[i];
				std::transform(pluginName.begin(), pluginName.end(), pluginName.begin(), tolower);
				if (qLower.empty() || pluginName.find(qLower) != std::string::npos) {
					if (ImGui::CollapsingHeader(m_names[i].c_str(), ImGuiTreeNodeFlags_DefaultOpen))
						m_plugins[i]->ShowOptions();
				}
			}
		}
	}
	bool PluginManager::ShowSystemVariables(PluginSystemVariableData* data, ShaderVariable::ValueType type)
	{
		bool ret = false;
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_isActive[i] && m_plugins[i]->HasSystemVariables((plugin::VariableType)type)) {
				int nameCount = m_plugins[i]->GetSystemVariableNameCount((plugin::VariableType)type);

				for (int j = 0; j < nameCount; j++) {
					const char* name = m_plugins[i]->GetSystemVariableName((plugin::VariableType)type, j);
					if (ImGui::Selectable(name)) {
						data->Owner = m_plugins[i];
						strcpy(data->Name, name);
						ret = true;
					}
				}
			}

		return ret;
	}
	bool PluginManager::ShowVariableFunctions(PluginFunctionData* data, ShaderVariable::ValueType type)
	{
		bool ret = false;
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_isActive[i] && m_plugins[i]->HasVariableFunctions((plugin::VariableType)type)) {
				int nameCount = m_plugins[i]->GetVariableFunctionNameCount((plugin::VariableType)type);

				for (int j = 0; j < nameCount; j++) {
					const char* name = m_plugins[i]->GetVariableFunctionName((plugin::VariableType)type, j);
					if (ImGui::Selectable(name)) {
						data->Owner = m_plugins[i];
						strcpy(data->Name, name);
						ret = true;
					}
				}
			}

		return ret;
	}
}