#include "PluginManager.h"
#include "../Logger.h"
#include "../Settings.h"
#include "../DefaultState.h"
#include "../SystemVariableManager.h"
#include "../../InterfaceManager.h"
#include "../../GUIManager.h"
#include "../../UI/CodeEditorUI.h"

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
	typedef int (*GetPluginVersionFn)();
	typedef const char* (*GetPluginNameFn)();

	void PluginManager::OnEvent(const SDL_Event& e)
	{
		for (const auto& plugin : m_plugins)
			plugin->OnEvent((void*)&e);
	}
	void PluginManager::Init(InterfaceManager* data, GUIManager* ui)
	{
		if (!ghc::filesystem::exists("./plugins/")) {
			ed::Logger::Get().Log("Directory for plugins doesn't exist");
			return;
		}

		ImGuiContext* uiCtx = ImGui::GetCurrentContext();

		for (const auto& entry : ghc::filesystem::directory_iterator("./plugins/")) {
			if (entry.is_directory()) {
				std::string pdir = entry.path().filename().native();

				// TODO: linux
#if defined(__linux__) || defined(__unix__)
				void* procDLL = dlopen(("./plugins/" + pdir + "/plugin.so").c_str(), RTLD_NOW);

				if (!procDLL) {
					ed::Logger::Get().Log("dlopen(\"" + pdir + "/plugin.so\") has failed.");
					continue;
				}

				// GetPluginAPIVersion() function
				GetPluginAPIVersionFn fnGetPluginAPIVersion = (GetPluginAPIVersionFn)dlsym(procDLL, "GetPluginAPIVersion");
				if (!fnGetPluginAPIVersion) {
					ed::Logger::Get().Log(pdir + "/plugin.so doesn't contain GetPluginAPIVersion.", true);
					dlclose(procDLL);
					continue;
				}

				int apiVer = (*fnGetPluginAPIVersion)();
				m_apiVersion.push_back(apiVer);

				// GetPluginAPIVersion() function
				GetPluginVersionFn fnGetPluginVersion = (GetPluginVersionFn)dlsym(procDLL, "GetPluginVersion");
				if (!fnGetPluginVersion) {
					ed::Logger::Get().Log(pdir + "/plugin.so doesn't contain GetPluginVersion.", true);
					dlclose(procDLL);
					continue;
				}

				int pluginVersion = (*fnGetPluginVersion)();
				m_pluginVersion.push_back(pluginVersion);

				// CreatePlugin() function
				CreatePluginFn fnCreatePlugin = (CreatePluginFn)dlsym(procDLL, "CreatePlugin");
				if (!fnCreatePlugin) {
					ed::Logger::Get().Log(pdir + "/plugin.so doesn't contain CreatePlugin.", true);
					dlclose(procDLL);
					continue;
				}

				// GetPluginName() function
				GetPluginNameFn fnGetPluginName = (GetPluginNameFn)dlsym(procDLL, "GetPluginName");
				if (!fnGetPluginName) {
					ed::Logger::Get().Log(pdir + "/plugin.so doesn't contain GetPluginName.", true);
					dlclose(procDLL);
					continue;
				}

				// create the actual plugin
				IPlugin* plugin = (*fnCreatePlugin)(uiCtx);
				if (plugin == nullptr) {
					ed::Logger::Get().Log(pdir + "/plugin.so CreatePlugin returned nullptr.", true);
					dlclose(procDLL);
					continue;
				}
#else
				HINSTANCE procDLL = LoadLibraryA(std::string("./plugins/" + pdir + "/plugin.dll").c_str());

				if (!procDLL) {
					ed::Logger::Get().Log("LoadLibraryA(\"" + pdir + "/plugin.dll\") has failed.");
					continue;
				}

				// GetPluginAPIVersion() function
				GetPluginAPIVersionFn fnGetPluginAPIVersion = (GetPluginAPIVersionFn)GetProcAddress(procDLL, "GetPluginAPIVersion");
				if (!fnGetPluginAPIVersion) {
					ed::Logger::Get().Log(pdir + "/plugin.dll doesn't contain GetPluginAPIVersion.", true);
					FreeLibrary(procDLL);
					continue;
				}

				int apiVer = (*fnGetPluginAPIVersion)();
				m_apiVersion.push_back(apiVer);

				// GetPluginVersion() function
				GetPluginVersionFn fnGetPluginVersion = (GetPluginVersionFn)GetProcAddress(procDLL, "GetPluginVersion");
				if (!fnGetPluginVersion) {
					ed::Logger::Get().Log(pdir + "/plugin.dll doesn't contain GetPluginVersion.", true);
					FreeLibrary(procDLL);
					continue;
				}

				int pluginVer = (*fnGetPluginVersion)();
				m_pluginVersion.push_back(pluginVer);

				// CreatePlugin() function
				CreatePluginFn fnCreatePlugin = (CreatePluginFn)GetProcAddress(procDLL, "CreatePlugin");
				if (!fnCreatePlugin) {
					ed::Logger::Get().Log(pdir + "/plugin.dll doesn't contain CreatePlugin.", true);
					FreeLibrary(procDLL);
					continue;
				}

				// GetPluginName() function
				GetPluginNameFn fnGetPluginName = (GetPluginNameFn)GetProcAddress(procDLL, "GetPluginName");
				if (!fnGetPluginName) {
					ed::Logger::Get().Log(pdir + "/plugin.dll doesn't contain GetPluginName.", true);
					FreeLibrary(procDLL);
					continue;
				}

				// create the actual plugin
				IPlugin* plugin = (*fnCreatePlugin)(uiCtx);
				if (plugin == nullptr) {
					ed::Logger::Get().Log(pdir + "/plugin.dll CreatePlugin returned nullptr.", true);
					FreeLibrary(procDLL);
					continue;
				}
#endif

				// list of loaded plugins
				std::vector<std::string> notLoaded = Settings::Instance().Plugins.NotLoaded;
				std::string pname = (*fnGetPluginName)();

				// set up pointers to app functions
				plugin->ObjectManager = (void*)&data->Objects;
				plugin->PipelineManager = (void*)&data->Pipeline;
				plugin->Renderer = (void*)&data->Renderer;
				plugin->Messages = (void*)&data->Messages;
				plugin->Project = (void*)&data->Parser;
				plugin->CodeEditor = (void*)(ui->Get(ViewID::Code));

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
				plugin->AddMessage = [](void* messages, plugin::MessageType mtype, const char* group, const char* txt, int ln) {
					MessageStack* msgs = (MessageStack*)messages;
					msgs->Add((MessageStack::Type)mtype, group, txt, ln);
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
				plugin->GetProjectDirectory = [](void* project) -> const char* {
					ProjectParser* proj = (ProjectParser*)project;
					return proj->GetProjectDirectory().c_str();
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
				plugin->GetWindowColorTexture = [](void* renderer) -> unsigned int {
					RenderEngine* rend = (RenderEngine*)renderer;
					return rend->GetTexture();
				};
				plugin->GetWindowDepthTexture = [](void* renderer) -> unsigned int {
					RenderEngine* rend = (RenderEngine*)renderer;
					return rend->GetDepthTexture();
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
				plugin->GetDPI = []() -> float {
					return Settings::Instance().DPIScale;
				};
				plugin->FileExists = [](void* project, const char* filename) -> bool {
					ProjectParser* proj = (ProjectParser*)project;
					return proj->FileExists(filename);
				};
				plugin->ClearMessageGroup = [](void* project, const char* group) {
					MessageStack* msgs = (MessageStack*)project;
					msgs->ClearGroup(group);
				};
				plugin->Log = [](const char* msg, bool error, const char* file, int line) {
					printf(msg);
					//ed::Logger::Get().Log(msg, error, file, line);
				};
				plugin->GetObjectCount = [](void* objects) -> int {
					ObjectManager* obj = (ObjectManager*)objects;
					return obj->GetObjects().size();
				};
				plugin->GetObjectName = [](void* objects, int index) -> const char* {
					ObjectManager* obj = (ObjectManager*)objects;
					return obj->GetObjects()[index].c_str();
				};
				plugin->IsTexture = [](void* objects, const char* name) -> bool {
					ObjectManager* obj = (ObjectManager*)objects;
					const auto& itemList = obj->GetItemDataList();
					const auto& itemNames = obj->GetObjects();
					int nameIndex = 0;

					for (const auto& item : itemList) {
						if (itemNames[nameIndex] == name)
								return item->IsTexture;
						nameIndex++;
					}

					return false;
				};
				plugin->GetTexture = [](void* objects, const char* name) -> unsigned int {
					ObjectManager* obj = (ObjectManager*)objects;
					return obj->GetTexture(name);
				};
				plugin->GetFlippedTexture = [](void* objects, const char* name) -> unsigned int {
					ObjectManager* obj = (ObjectManager*)objects;
					return obj->GetFlippedTexture(name);
				};
				plugin->GetTextureSize = [](void* objects, const char* name, int& w, int& h) {
					ObjectManager* obj = (ObjectManager*)objects;
					glm::ivec2 tsize = obj->GetTextureSize(name);
					w = tsize.x;
					h = tsize.y;
				};
				plugin->BindDefaultState = []() {
					DefaultState::Bind();
				};
				plugin->OpenInCodeEditor = [](void* codeed, void* item, const char* filename, int id) {
					CodeEditorUI* editor = (CodeEditorUI*)codeed;
					editor->OpenPluginCode((PipelineItem*)item, filename, id);
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

	void PluginManager::BeginRender()
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_isActive[i])
				m_plugins[i]->BeginRender();
	}
	void PluginManager::EndRender()
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_isActive[i])
				m_plugins[i]->EndRender();
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
				return m_pluginVersion[i];

		return 0;
	}
	int PluginManager::GetPluginAPIVersion(const std::string& plugin)
	{
		for (int i = 0; i < m_names.size(); i++)
			if (m_names[i] == plugin)
				return m_apiVersion[i];

		return 0;
	}
	bool PluginManager::IsActive(const std::string& plugin)
	{
		for (int i = 0; i < m_names.size(); i++)
			if (m_names[i] == plugin)
				return m_isActive[i];

		return false;
	}

	void PluginManager::HandleDropFile(const char* filename)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_isActive[i] && m_plugins[i]->HandleDropFile(filename))
				break;
	}

	void PluginManager::ShowContextItems(const std::string& menu, void* owner, void* extraData)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_isActive[i] && m_plugins[i]->HasContextItems(menu.c_str())) {
				ImGui::Separator();
				m_plugins[i]->ShowContextItems(menu.c_str(), owner, extraData);
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