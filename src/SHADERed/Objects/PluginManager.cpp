#include <SHADERed/GUIManager.h>
#include <SHADERed/InterfaceManager.h>
#include <SHADERed/Objects/DefaultState.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/PluginManager.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/ObjectPreviewUI.h>
#include <SHADERed/UI/PinnedUI.h>
#include <SHADERed/UI/PipelineUI.h>
#include <SHADERed/UI/PreviewUI.h>
#include <SHADERed/UI/PropertyUI.h>
#include <SHADERed/UI/UIHelper.h>

#include <misc/ImFileDialog.h>

#include <imgui/imgui.h>
#include <vector>
#include <fstream>
#include <algorithm>
#include <filesystem>

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace ed {
	typedef IPlugin1* (*CreatePluginFn)();
	typedef void (*DestroyPluginFn)(IPlugin1* plugin);
	typedef int (*GetPluginAPIVersionFn)();
	typedef int (*GetPluginVersionFn)();
	typedef const char* (*GetPluginNameFn)();

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
	void ptrFreeLibrary(void* so)
	{
		dlclose(so);
	}
#else
	void ptrFreeLibrary(HINSTANCE dll)
	{
		FreeLibrary(dll);
	}
#endif

	void PluginManager::OnEvent(const SDL_Event& e)
	{
		for (const auto& plugin : m_plugins)
			plugin->OnEvent((void*)&e);
	}
	void PluginManager::Init(InterfaceManager* data, GUIManager* ui)
	{
		m_data = data;
		m_ui = ui;
		
		std::string pluginsDirLoc = Settings::Instance().ConvertPath("plugins/");

		if (!std::filesystem::exists(pluginsDirLoc)) {
			ed::Logger::Get().Log("Directory for plugins doesn't exist");
			return;
		}

		std::string settingsFileLoc = ed::Settings::Instance().ConvertPath("data/plugin_settings.ini");

		std::ifstream ini(settingsFileLoc);
		std::copy(std::istream_iterator<std::string>(ini),
			std::istream_iterator<std::string>(),
			std::back_inserter(m_iniLines));
		ini.close();

		std::string pluginExt = "dll";
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
		pluginExt = "so";
#endif
		
		std::vector<std::string> allNames;
		std::vector<std::string>& notLoaded = Settings::Instance().Plugins.NotLoaded;

		for (const auto& entry : std::filesystem::directory_iterator(pluginsDirLoc)) {
			if (entry.is_directory()) {
				std::string pdir = entry.path().filename().string();

				Logger::Get().Log("Loading \"" + pdir + "\" plugin.");

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
				void* procDLL = dlopen((pluginsDirLoc + pdir + "/plugin.so").c_str(), RTLD_NOW);

				if (!procDLL) {
					ed::Logger::Get().Log("dlopen(\"" + pdir + "/plugin.so\") has failed.");
					continue;
				}

				// global functions
				GetPluginAPIVersionFn fnGetPluginAPIVersion = (GetPluginAPIVersionFn)dlsym(procDLL, "GetPluginAPIVersion");
				GetPluginVersionFn fnGetPluginVersion = (GetPluginVersionFn)dlsym(procDLL, "GetPluginVersion");
				CreatePluginFn fnCreatePlugin = (CreatePluginFn)dlsym(procDLL, "CreatePlugin");
				GetPluginNameFn fnGetPluginName = (GetPluginNameFn)dlsym(procDLL, "GetPluginName");
#else
				HINSTANCE procDLL = LoadLibraryA(std::string("./plugins/" + pdir + "/plugin.dll").c_str());

				if (!procDLL) {
					DWORD test = GetLastError();
					ed::Logger::Get().Log("LoadLibraryA(\"" + pdir + "/plugin.dll\") has failed.");
					continue;
				}

				// global functions
				GetPluginAPIVersionFn fnGetPluginAPIVersion = (GetPluginAPIVersionFn)GetProcAddress(procDLL, "GetPluginAPIVersion");
				GetPluginVersionFn fnGetPluginVersion = (GetPluginVersionFn)GetProcAddress(procDLL, "GetPluginVersion");
				CreatePluginFn fnCreatePlugin = (CreatePluginFn)GetProcAddress(procDLL, "CreatePlugin");
				GetPluginNameFn fnGetPluginName = (GetPluginNameFn)GetProcAddress(procDLL, "GetPluginName");
#endif

				// GetPluginName() function
				if (!fnGetPluginName) {
					ed::Logger::Get().Log(pdir + "/plugin." + pluginExt + " doesn't contain GetPluginName.", true);
					(*ptrFreeLibrary)(procDLL);
					continue;
				}
				std::string pname = (*fnGetPluginName)();
				allNames.push_back(pname);

				// check if this plugin should be loaded
				if (std::count(notLoaded.begin(), notLoaded.end(), pname) > 0) {
					(*ptrFreeLibrary)(procDLL);
					continue;
				}

				// GetPluginAPIVersion()
				if (!fnGetPluginAPIVersion) {
					ed::Logger::Get().Log(pdir + "/plugin." + pluginExt + " doesn't contain GetPluginAPIVersion.", true);
					(*ptrFreeLibrary)(procDLL);
					continue;
				}

				int apiVer = (*fnGetPluginAPIVersion)();
				if (apiVer != CURRENT_PLUGINAPI_VERSION) {
					ed::Logger::Get().Log(pdir + "/plugin." + pluginExt + " uses newer/older plugin API version. Please update the plugin or update SHADERed.", true);
					(*ptrFreeLibrary)(procDLL);
					if (std::count(notLoaded.begin(), notLoaded.end(), pname) == 0)
						m_incompatible.push_back(pname);
					continue;
				}

				// TODO: ImGui::DebugCheckVersionAndDataLayout()

				// GetPluginVersion() function
				if (!fnGetPluginVersion) {
					ed::Logger::Get().Log(pdir + "/plugin." + pluginExt + " doesn't contain GetPluginVersion.", true);
					(*ptrFreeLibrary)(procDLL);
					continue;
				}

				int pluginVer = (*fnGetPluginVersion)();

				// CreatePlugin() function
				if (!fnCreatePlugin) {
					ed::Logger::Get().Log(pdir + "/plugin." + pluginExt + " doesn't contain CreatePlugin.", true);
					(*ptrFreeLibrary)(procDLL);
					continue;
				}

				// create the actual plugin
				IPlugin1* plugin = (*fnCreatePlugin)();
				if (plugin == nullptr) {
					ed::Logger::Get().Log(pdir + "/plugin." + pluginExt + " CreatePlugin returned nullptr.", true);
					(*ptrFreeLibrary)(procDLL);
					continue;
				}
				
				// register plugin into the manager
				RegisterPlugin(plugin, pname, apiVer, pluginVer, procDLL);
			}
		}


		// check if plugins listed in not loaded even exist
		for (int i = 0; i < notLoaded.size(); i++) {
			bool exists = false;
			for (int j = 0; j < allNames.size(); j++) {
				if (notLoaded[i] == allNames[j]) {
					exists = true;
					break;
				}
			}
			if (!exists) {
				notLoaded.erase(notLoaded.begin() + i);
				i--;
			}
		}
	}
	void PluginManager::Destroy()
	{
		std::string settingsFileLoc = ed::Settings::Instance().ConvertPath("data/plugin_settings.ini");

		std::ofstream ini(settingsFileLoc);

		for (int i = m_plugins.size() - 1; i >= 0; i--) {
			Logger::Get().Log("Destroying \"" + m_names[i] + "\" plugin.");

			int optc = m_plugins[i]->Options_GetCount();
			if (optc) {
				ini << "[" << m_names[i] << "]" << std::endl;
				for (int j = 0; j < optc; j++) {
					const char* key = m_plugins[i]->Options_GetKey(j);
					const char* val = m_plugins[i]->Options_GetValue(j);

					ini << key << "=" << val << std::endl;
				}
			}

			m_plugins[i]->Destroy();
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
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

		ini.close();
		m_plugins.clear();
	}
	void PluginManager::Update(float delta)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			m_plugins[i]->Update(delta);
	}

	void PluginManager::BeginRender()
	{
		for (int i = 0; i < m_plugins.size(); i++)
			m_plugins[i]->BeginRender();
	}
	void PluginManager::EndRender()
	{
		for (int i = 0; i < m_plugins.size(); i++)
			m_plugins[i]->EndRender();
	}

	std::vector<InputLayoutItem> PluginManager::BuildInputLayout(IPlugin1* plugin, const char* type, void* pldata)
	{
		int size = plugin->PipelineItem_GetInputLayoutSize(type, pldata);
		std::vector<InputLayoutItem> inpLayout;
		for (int j = 0; j < size; j++) {
			plugin::InputLayoutItem inpOut;
			plugin->PipelineItem_GetInputLayoutItem(type, pldata, j, inpOut);

			InputLayoutItem inp;
			inp.Value = (InputLayoutValue)inpOut.Value;
			inp.Semantic = std::string(inpOut.Semantic);

			inpLayout.push_back(inp);
		}

		return inpLayout;
	}

	void PluginManager::RegisterPlugin(IPlugin1* plugin, const std::string pname, int apiVer, int pluginVer, void* procDLL)
	{
		// set up pointers to app functions
		plugin->ObjectManager = (void*)&m_data->Objects;
		plugin->PipelineManager = (void*)&m_data->Pipeline;
		plugin->Renderer = (void*)&m_data->Renderer;
		plugin->Messages = (void*)&m_data->Messages;
		plugin->Project = (void*)&m_data->Parser;
		plugin->Debugger = (void*)&m_data->Debugger;
		plugin->UI = (void*)m_ui;
		plugin->Plugins = (void*)this;

		plugin->AddObject = [](void* objectManager, const char* name, const char* type, void* data, unsigned int id, void* owner) {
			ObjectManager* objm = (ObjectManager*)objectManager;
			objm->CreatePluginItem(name, type, data, id, (IPlugin1*)owner);
		};
		plugin->AddCustomPipelineItem = [](void* pipeManager, void* parentPtr, const char* name, const char* type, void* data, void* owner) -> bool {
			PipelineManager* pipe = (PipelineManager*)pipeManager;
			PipelineItem* parent = (PipelineItem*)parentPtr;
			char* parentName = nullptr;

			if (parent != nullptr)
				parentName = parent->Name;

			return pipe->AddPluginItem(parentName, name, type, data, (IPlugin1*)owner);
		};
		plugin->AddMessage = [](void* messages, plugin::MessageType mtype, const char* group, const char* txt, int ln) {
			MessageStack* msgs = (MessageStack*)messages;
			std::string groupStr = "";
			if (group != nullptr)
				groupStr = std::string(group);
			msgs->Add((MessageStack::Type)mtype, groupStr, txt, ln);
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
			objs->ResizeRenderTexture(objs->Get(name), glm::ivec2(width, height));
		};
		plugin->ResizeImage = [](void* objects, const char* name, int width, int height) {
			ObjectManager* objs = (ObjectManager*)objects;
			objs->ResizeImage(objs->Get(name), glm::ivec2(width, height));
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
		plugin->OpenProject = [](void* uiData, const char* filename) {
			GUIManager* ui = (GUIManager*)uiData;
			ui->Open(filename);
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
			PipelineItem* pitem = (PipelineItem*)shaderpass;

			if (pitem != nullptr && pitem->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)pitem->Data;
				pass->Variables.Bind(item);
			}
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
		plugin->SetTime = [](float time) {
			float curTime = SystemVariableManager::Instance().GetTime();
			SystemVariableManager::Instance().AdvanceTimer(time - curTime);
		};
		plugin->SetGeometryTransform = [](void* item, float scale[3], float rota[3], float pos[3]) {
			SystemVariableManager::Instance().SetGeometryTransform((PipelineItem*)item, glm::make_vec3(scale), glm::make_vec3(rota), glm::make_vec3(pos));
		};
		plugin->SetMousePosition = [](float x, float y) {
			SystemVariableManager::Instance().SetMousePosition(x, y);
		};
		plugin->SetKeysWASD = [](bool w, bool a, bool s, bool d) {
			SystemVariableManager::Instance().SetKeysWASD(w, a, s, d);
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
			printf("%s\n", msg);
			//ed::Logger::Get().Log(msg, error, file, line);
		};
		plugin->GetObjectCount = [](void* objects) -> int {
			ObjectManager* obj = (ObjectManager*)objects;
			return obj->GetObjects().size();
		};
		plugin->GetObjectName = [](void* objects, int index) -> const char* {
			ObjectManager* obj = (ObjectManager*)objects;
			return obj->GetObjects()[index]->Name.c_str();
		};
		plugin->IsTexture = [](void* objects, const char* name) -> bool {
			ObjectManager* obj = (ObjectManager*)objects;
			return obj->Get(name)->Type == ObjectType::Texture;
		};
		plugin->GetTexture = [](void* objects, const char* name) -> unsigned int {
			ObjectManager* obj = (ObjectManager*)objects;
			return obj->Get(name)->Texture;
		};
		plugin->GetFlippedTexture = [](void* objects, const char* name) -> unsigned int {
			ObjectManager* obj = (ObjectManager*)objects;
			return obj->Get(name)->FlippedTexture;
		};
		plugin->GetTextureSize = [](void* objects, const char* name, int& w, int& h) {
			ObjectManager* obj = (ObjectManager*)objects;
			glm::ivec2 tsize = obj->Get(name)->TextureSize;
			w = tsize.x;
			h = tsize.y;
		};
		plugin->BindDefaultState = []() {
			DefaultState::Bind();
		};
		plugin->OpenInCodeEditor = [](void* ui, void* item, const char* filename, int id) {
			CodeEditorUI* editor = (CodeEditorUI*)((GUIManager*)ui)->Get(ViewID::Code);
			editor->OpenPluginCode((PipelineItem*)item, filename, id);
		};
		plugin->GetPipelineItemCount = [](void* pipeline) -> int {
			PipelineManager* pipe = (PipelineManager*)pipeline;
			return pipe->GetList().size();
		};
		plugin->GetPipelineItemType = [](void* item) -> plugin::PipelineItemType {
			return (plugin::PipelineItemType)((PipelineItem*)item)->Type;
		};
		plugin->GetPipelineItemByIndex = [](void* pipeline, int index) -> void* {
			PipelineManager* pipe = (PipelineManager*)pipeline;
			return (void*)pipe->GetList()[index];
		};
		plugin->DEPRECATED_GetOpenDirectoryDialog = [](char* out) -> bool {
			return false;
		};
		plugin->DEPRECATED_GetOpenFileDialog = [](char* out, const char* files) -> bool {
			return false;
		};
		plugin->DEPRECATED_GetSaveFileDialog = [](char* out, const char* files) -> bool {
			return false;
		};
		plugin->GetIncludePathCount = []() -> int {
			return Settings::Instance().Project.IncludePaths.size();
		};
		plugin->GetIncludePath = [](void* project, int index) -> const char* {
			return Settings::Instance().Project.IncludePaths[index].c_str();
		};
		plugin->GetMessagesCurrentItem = [](void* messages) -> const char* {
			return ((ed::MessageStack*)messages)->CurrentItem.c_str();
		};
		plugin->OnEditorContentChange = nullptr; // will be set by CodeEditorUI
		plugin->GetPipelineItemSPIRV = [](void* item, plugin::ShaderStage stage, int* len) -> unsigned int* {
			PipelineItem* pItem = (PipelineItem*)item;
			if (pItem->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* data = (pipe::ShaderPass*)pItem->Data;

				if (stage == plugin::ShaderStage::Pixel) {
					*len = data->PSSPV.size();
					return data->PSSPV.data();
				} else if (stage == plugin::ShaderStage::Vertex) {
					*len = data->VSSPV.size();
					return data->VSSPV.data();
				} else if (stage == plugin::ShaderStage::Geometry) {
					*len = data->GSSPV.size();
					return data->GSSPV.data();
				} else if (stage == plugin::ShaderStage::TessellationControl) {
					*len = data->TCSSPV.size();
					return data->TCSSPV.data();
				} else if (stage == plugin::ShaderStage::TessellationEvaluation) {
					*len = data->TESSPV.size();
					return data->TESSPV.data();
				}
			} else if (pItem->Type == PipelineItem::ItemType::ComputePass) {
				pipe::ComputePass* data = (pipe::ComputePass*)pItem->Data;

				*len = data->SPV.size();
				return data->SPV.data();
			}

			*len = 0;
			return nullptr;
		};
		plugin->RegisterShortcut = [](void* plugin, const char* name) {
			KeyboardShortcuts::Instance().RegisterPluginShortcut((IPlugin1*)plugin, std::string(name));
		};
		plugin->GetSettingsBoolean = [](const char* name) -> bool {
			const Settings& seti = Settings::Instance();

			std::string lwr(name);
			std::transform(lwr.begin(), lwr.end(), lwr.begin(), tolower);

			/* GENERAL */
			if (lwr == "vsync") return seti.General.VSync;
			if (lwr == "autoopenerrorwindow") return seti.General.AutoOpenErrorWindow;
			if (lwr == "toolbar") return seti.General.Toolbar;
			if (lwr == "recovery") return seti.General.Recovery;
			if (lwr == "checkupdates") return seti.General.CheckUpdates;
			if (lwr == "recompileonfilechange") return seti.General.RecompileOnFileChange;
			if (lwr == "autorecompile") return seti.General.AutoRecompile;
			if (lwr == "autouniforms") return seti.General.AutoUniforms;
			if (lwr == "autouniformspin") return seti.General.AutoUniformsPin;
			if (lwr == "autouniformsfunction") return seti.General.AutoUniformsFunction;
			if (lwr == "autouniformsdelete") return seti.General.AutoUniformsDelete;
			if (lwr == "reopenshaders") return seti.General.ReopenShaders;
			if (lwr == "useexternaleditor") return seti.General.UseExternalEditor;
			if (lwr == "openshadersondblclk") return seti.General.OpenShadersOnDblClk;
			if (lwr == "itempropsondblclk") return seti.General.ItemPropsOnDblCLk;
			if (lwr == "selectitemondblclk") return seti.General.SelectItemOnDblClk;
			if (lwr == "log") return seti.General.Log;
			if (lwr == "streamlogs") return seti.General.StreamLogs;
			if (lwr == "pipelogstoterminal") return seti.General.PipeLogsToTerminal;
			if (lwr == "autoscale") return seti.General.AutoScale;
			if (lwr == "tips") return seti.General.Tips;

			/* EDITOR */
			if (lwr == "smartpredictions") return seti.Editor.SmartPredictions;
			if (lwr == "activesmartpredictions") return seti.Editor.ActiveSmartPredictions;
			if (lwr == "showwhitespace") return seti.Editor.ShowWhitespace;
			if (lwr == "highlightcurrentline") return seti.Editor.HiglightCurrentLine;
			if (lwr == "linenumbers") return seti.Editor.LineNumbers;
			if (lwr == "statusbar") return seti.Editor.StatusBar;
			if (lwr == "horizontalscroll") return seti.Editor.HorizontalScroll;
			if (lwr == "autobracecompletion") return seti.Editor.AutoBraceCompletion;
			if (lwr == "smartindent") return seti.Editor.SmartIndent;
			if (lwr == "autoindentonpaste") return seti.Editor.AutoIndentOnPaste;
			if (lwr == "insertspaces") return seti.Editor.InsertSpaces;
			if (lwr == "functiontooltips") return seti.Editor.FunctionTooltips;
			if (lwr == "syntaxhighlighting") return seti.Editor.SyntaxHighlighting;

			/* DEBUG */
			if (lwr == "showvaluesonhover") return seti.Debug.ShowValuesOnHover;
			if (lwr == "autofetch") return seti.Debug.AutoFetch;
			if (lwr == "primitiveoutline") return seti.Debug.PrimitiveOutline;
			if (lwr == "pixeloutline") return seti.Debug.PixelOutline;

			/* PREVIEW */
			if (lwr == "pausedonstartup") return seti.Preview.PausedOnStartup;
			if (lwr == "switchleftrightclick") return seti.Preview.SwitchLeftRightClick;
			if (lwr == "hidemenuinperformancemode") return seti.Preview.HideMenuInPerformanceMode;
			if (lwr == "boundingbox") return seti.Preview.BoundingBox;
			if (lwr == "gizmo") return seti.Preview.Gizmo;
			if (lwr == "gizmorotationui") return seti.Preview.GizmoRotationUI;
			if (lwr == "propertypick") return seti.Preview.PropertyPick;
			if (lwr == "statusbar") return seti.Preview.StatusBar;
			if (lwr == "applyfpslimittoapp") return seti.Preview.ApplyFPSLimitToApp;
			if (lwr == "lostfocuslimitfps") return seti.Preview.LostFocusLimitFPS;

			/* PROJECT */
			if (lwr == "fpcamera") return seti.Project.FPCamera;
			if (lwr == "usealphachannel") return seti.Project.UseAlphaChannel;

			return false;
		};
		plugin->GetSettingsInteger = [](const char* name) -> int {
			const Settings& seti = Settings::Instance();

			std::string lwr(name);
			std::transform(lwr.begin(), lwr.end(), lwr.begin(), tolower);

			/* GENERAL */
			if (lwr == "fontsize") return seti.General.FontSize;

			/* EDITOR */
			if (lwr == "editorfontsize") return seti.Editor.FontSize;
			if (lwr == "tabsize") return seti.Editor.TabSize;

			/* PREVIEW */
			if (lwr == "gizmosnaptranslation") return seti.Preview.GizmoSnapTranslation;
			if (lwr == "gizmosnapscale") return seti.Preview.GizmoSnapScale;
			if (lwr == "gizmosnaprotation") return seti.Preview.GizmoSnapRotation;
			if (lwr == "fpslimit") return seti.Preview.FPSLimit;
			if (lwr == "msaa") return seti.Preview.MSAA;

			return -1;
		};
		plugin->GetSettingsString = [](const char* name) -> const char* {
			const Settings& seti = Settings::Instance();

			std::string lwr(name);
			std::transform(lwr.begin(), lwr.end(), lwr.begin(), tolower);

			/* GENERAL */
			if (lwr == "startuptemplate") return seti.General.StartUpTemplate.c_str();
			if (lwr == "font") return seti.General.Font;

			/* EDITOR */
			if (lwr == "editorfont") return seti.Editor.Font;

			return nullptr;
		};
		plugin->GetSettingsFloat = [](const char* name) -> float {
			const Settings& seti = Settings::Instance();

			std::string lwr(name);
			std::transform(lwr.begin(), lwr.end(), lwr.begin(), tolower);

			if (lwr == "clearcolorr") return seti.Project.ClearColor.r;
			if (lwr == "clearcolorg") return seti.Project.ClearColor.g;
			if (lwr == "clearcolorb") return seti.Project.ClearColor.b;
			if (lwr == "clearcolora") return seti.Project.ClearColor.a;

			return 0.0f;
		};
		plugin->GetPreviewUIRect = [](void* ui, float* out) {
			PreviewUI* preview = (PreviewUI*)(((GUIManager*)ui)->Get(ViewID::Preview));
			glm::vec2 pos = preview->GetUIRectPosition();
			glm::vec2 size = preview->GetUIRectSize();
			out[0] = pos.x;
			out[1] = pos.y;
			out[2] = size.x;
			out[3] = size.y;
		};
		plugin->GetPlugin = [](void* pluginManager, const char* name) -> void* {
			return (void*)((PluginManager*)pluginManager)->GetPlugin(name);
		};
		plugin->GetPluginListSize = [](void* pluginManager) -> int {
			return ((PluginManager*)pluginManager)->Plugins().size();
		};
		plugin->GetPluginName = [](void* pluginManager, int index) -> const char* {
			PluginManager* pl = (PluginManager*)pluginManager;
			if (index >= pl->Plugins().size() || index <= 0)
				return nullptr;
			return pl->GetPluginName(pl->Plugins()[index]).c_str();
		};
		plugin->SendPluginMessage = [](void* pluginManager, void* plugin, const char* receiver, char* msg, int msgLen) {
			PluginManager* pl = (PluginManager*)pluginManager;
			IPlugin1* receiverPlugin = pl->GetPlugin(receiver);

			if (receiverPlugin)
				receiverPlugin->HandlePluginMessage(pl->GetPluginName((IPlugin1*)plugin).c_str(), msg, msgLen);
		};
		plugin->BroadcastPluginMessage = [](void* pluginManager, void* plugin, char* msg, int msgLen) {
			PluginManager* pl = (PluginManager*)pluginManager;
			const char* myName = pl->GetPluginName((IPlugin1*)plugin).c_str();
			for (auto& p : pl->Plugins())
				p->HandlePluginMessage(myName, msg, msgLen);
		};
		plugin->ToggleFullscreen = [](void* UI) {
			SDL_Window* wnd = ((GUIManager*)UI)->GetSDLWindow();
			Uint32 wndFlags = SDL_GetWindowFlags(wnd);
			bool isFullscreen = wndFlags & SDL_WINDOW_FULLSCREEN_DESKTOP;
			SDL_SetWindowFullscreen(wnd, (!isFullscreen) * SDL_WINDOW_FULLSCREEN_DESKTOP);
		};
		plugin->IsFullscreen = [](void* UI) -> bool {
			SDL_Window* wnd = ((GUIManager*)UI)->GetSDLWindow();
			return SDL_GetWindowFlags(wnd) & SDL_WINDOW_FULLSCREEN_DESKTOP;
		};
		plugin->TogglePerformanceMode = [](void* UI) {
			((GUIManager*)UI)->SetPerformanceMode(!((GUIManager*)UI)->IsPerformanceMode());
		};
		plugin->IsInPerformanceMode = [](void* UI) -> bool {
			return ((GUIManager*)UI)->IsPerformanceMode();
		};
		plugin->GetPipelineItemName = [](void* item) -> const char* {
			return ((PipelineItem*)item)->Name;
		};
		plugin->GetPipelineItemPluginOwner = [](void* item) -> void* {
			PipelineItem* pitem = ((PipelineItem*)item);
			if (pitem->Type == PipelineItem::ItemType::PluginItem)
				return ((pipe::PluginItemData*)pitem->Data)->Owner;
			return nullptr;
		};
		plugin->GetPipelineItemVariableCount = [](void* item) -> int {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)pitem->Data;
				return pass->Variables.GetVariables().size();
			} else if (pitem->Type == PipelineItem::ItemType::ComputePass) {
				pipe::ComputePass* pass = (pipe::ComputePass*)pitem->Data;
				return pass->Variables.GetVariables().size();
			} else if (pitem->Type == PipelineItem::ItemType::AudioPass) {
				pipe::AudioPass* pass = (pipe::AudioPass*)pitem->Data;
				return pass->Variables.GetVariables().size();
			}

			return 0;
		};
		plugin->GetPipelineItemVariableName = [](void* item, int index) -> const char* {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)pitem->Data;
				return pass->Variables.GetVariables()[index]->Name;
			} else if (pitem->Type == PipelineItem::ItemType::ComputePass) {
				pipe::ComputePass* pass = (pipe::ComputePass*)pitem->Data;
				return pass->Variables.GetVariables()[index]->Name;
			} else if (pitem->Type == PipelineItem::ItemType::AudioPass) {
				pipe::AudioPass* pass = (pipe::AudioPass*)pitem->Data;
				return pass->Variables.GetVariables()[index]->Name;
			}

			return nullptr;
		};
		plugin->GetPipelineItemVariableValue = [](void* item, int index) -> char* {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)pitem->Data;
				return pass->Variables.GetVariables()[index]->Data;
			} else if (pitem->Type == PipelineItem::ItemType::ComputePass) {
				pipe::ComputePass* pass = (pipe::ComputePass*)pitem->Data;
				return pass->Variables.GetVariables()[index]->Data;
			} else if (pitem->Type == PipelineItem::ItemType::AudioPass) {
				pipe::AudioPass* pass = (pipe::AudioPass*)pitem->Data;
				return pass->Variables.GetVariables()[index]->Data;
			}

			return nullptr;
		};
		plugin->GetPipelineItemVariableType = [](void* item, int index) -> plugin::VariableType {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)pitem->Data;
				return (plugin::VariableType)pass->Variables.GetVariables()[index]->GetType();
			} else if (pitem->Type == PipelineItem::ItemType::ComputePass) {
				pipe::ComputePass* pass = (pipe::ComputePass*)pitem->Data;
				return (plugin::VariableType)pass->Variables.GetVariables()[index]->GetType();
			} else if (pitem->Type == PipelineItem::ItemType::AudioPass) {
				pipe::AudioPass* pass = (pipe::AudioPass*)pitem->Data;
				return (plugin::VariableType)pass->Variables.GetVariables()[index]->GetType();
			}

			return plugin::VariableType::Float1;
		};
		plugin->AddPipelineItemVariable = [](void* item, const char* name, plugin::VariableType type) -> bool {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)pitem->Data;
				if (pass->Variables.ContainsVariable(name)) return false;
				pass->Variables.AddCopy(ed::ShaderVariable((ed::ShaderVariable::ValueType)type, name));
				return true;
			} else if (pitem->Type == PipelineItem::ItemType::ComputePass) {
				pipe::ComputePass* pass = (pipe::ComputePass*)pitem->Data;
				if (pass->Variables.ContainsVariable(name)) return false;
				pass->Variables.AddCopy(ed::ShaderVariable((ed::ShaderVariable::ValueType)type, name));
				return true;
			} else if (pitem->Type == PipelineItem::ItemType::AudioPass) {
				pipe::AudioPass* pass = (pipe::AudioPass*)pitem->Data;
				if (pass->Variables.ContainsVariable(name)) return false;
				pass->Variables.AddCopy(ed::ShaderVariable((ed::ShaderVariable::ValueType)type, name));
				return true;
			}

			return false;
		};
		plugin->GetPipelineItemChildrenCount = [](void* item) -> int {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)pitem->Data;
				return pass->Items.size();
			} else if (pitem->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* pass = (pipe::PluginItemData*)pitem->Data;
				return pass->Items.size();
			}

			return 0;
		};
		plugin->GetPipelineItemChild = [](void* item, int index) -> void* {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)pitem->Data;
				return pass->Items[index];
			} else if (pitem->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* pass = (pipe::PluginItemData*)pitem->Data;
				return pass->Items[index];
			}

			return nullptr;
		};
		plugin->SetPipelineItemPosition = [](void* item, float x, float y, float z) {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* item = (pipe::GeometryItem*)pitem->Data;
				item->Position = glm::vec3(x, y, z);
			} else if (pitem->Type == PipelineItem::ItemType::Model) {
				pipe::Model* item = (pipe::Model*)pitem->Data;
				item->Position = glm::vec3(x, y, z);
			}
		};
		plugin->SetPipelineItemRotation = [](void* item, float x, float y, float z) {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* item = (pipe::GeometryItem*)pitem->Data;
				item->Rotation = glm::vec3(x, y, z);
			} else if (pitem->Type == PipelineItem::ItemType::Model) {
				pipe::Model* item = (pipe::Model*)pitem->Data;
				item->Rotation = glm::vec3(x, y, z);
			}
		};
		plugin->SetPipelineItemScale = [](void* item, float x, float y, float z) {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* item = (pipe::GeometryItem*)pitem->Data;
				item->Scale = glm::vec3(x, y, z);
			} else if (pitem->Type == PipelineItem::ItemType::Model) {
				pipe::Model* item = (pipe::Model*)pitem->Data;
				item->Scale = glm::vec3(x, y, z);
			}
		};
		plugin->GetPipelineItemPosition = [](void* item, float* data) {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* item = (pipe::GeometryItem*)pitem->Data;
				data[0] = item->Position.x;
				data[1] = item->Position.y;
				data[2] = item->Position.z;
			} else if (pitem->Type == PipelineItem::ItemType::Model) {
				pipe::Model* item = (pipe::Model*)pitem->Data;
				data[0] = item->Position.x;
				data[1] = item->Position.y;
				data[2] = item->Position.z;
			}
		};
		plugin->GetPipelineItemRotation = [](void* item, float* data) {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* item = (pipe::GeometryItem*)pitem->Data;
				data[0] = item->Rotation.x;
				data[1] = item->Rotation.y;
				data[2] = item->Rotation.z;
			} else if (pitem->Type == PipelineItem::ItemType::Model) {
				pipe::Model* item = (pipe::Model*)pitem->Data;
				data[0] = item->Rotation.x;
				data[1] = item->Rotation.y;
				data[2] = item->Rotation.z;
			}
		};
		plugin->GetPipelineItemScale = [](void* item, float* data) {
			PipelineItem* pitem = ((PipelineItem*)item);

			if (pitem->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* item = (pipe::GeometryItem*)pitem->Data;
				data[0] = item->Scale.x;
				data[1] = item->Scale.y;
				data[2] = item->Scale.z;
			} else if (pitem->Type == PipelineItem::ItemType::Model) {
				pipe::Model* item = (pipe::Model*)pitem->Data;
				data[0] = item->Scale.x;
				data[1] = item->Scale.y;
				data[2] = item->Scale.z;
			}
		};
		plugin->PushNotification = [](void* UI, void* plugin, int id, const char* text, const char* btn) {
			((GUIManager*)UI)->AddNotification(
				id, text, btn, [](int id, IPlugin1* pl) {
					pl->HandleNotification(id);
				},
				(IPlugin1*)plugin);
		};
		plugin->DebuggerJump = [](void* Debugger, void* ed, int line) {
			((ed::DebugInformation*)Debugger)->Jump(line);
			int curLine = ((ed::DebugInformation*)Debugger)->GetCurrentLine();
			((TextEditor*)ed)->SetCurrentLineIndicator(curLine);
		};
		plugin->DebuggerContinue = [](void* Debugger, void* ed) {
			((ed::DebugInformation*)Debugger)->Continue();
			int curLine = ((ed::DebugInformation*)Debugger)->GetCurrentLine();
			((TextEditor*)ed)->SetCurrentLineIndicator(curLine);
		};
		plugin->DebuggerStep = [](void* Debugger, void* ed) {
			((ed::DebugInformation*)Debugger)->Step();
			int curLine = ((ed::DebugInformation*)Debugger)->GetCurrentLine();
			((TextEditor*)ed)->SetCurrentLineIndicator(curLine);
		};
		plugin->DebuggerStepInto = [](void* Debugger, void* ed) {
			((ed::DebugInformation*)Debugger)->StepInto();
			int curLine = ((ed::DebugInformation*)Debugger)->GetCurrentLine();
			((TextEditor*)ed)->SetCurrentLineIndicator(curLine);
		};
		plugin->DebuggerStepOut = [](void* Debugger, void* ed) {
			((ed::DebugInformation*)Debugger)->StepOut();
			int curLine = ((ed::DebugInformation*)Debugger)->GetCurrentLine();
			((TextEditor*)ed)->SetCurrentLineIndicator(curLine);
		};
		plugin->DebuggerCheckBreakpoint = [](void* Debugger, void* ed, int line) -> bool {
			return ((ed::DebugInformation*)Debugger)->CheckBreakpoint(line);
		};
		plugin->DebuggerIsDebugging = [](void* Debugger, void* ed) -> bool {
			return ((ed::DebugInformation*)Debugger)->IsDebugging();
		};
		plugin->DebuggerGetCurrentLine = [](void* Debugger) -> int {
			return ((ed::DebugInformation*)Debugger)->GetCurrentLine();
		};
		plugin->IsRenderTexture = [](void* objects, const char* name) -> bool {
			ObjectManager* obj = (ObjectManager*)objects;
			ObjectManagerItem* item = obj->Get(name);

			if (item && item->RT != nullptr)
				return true;

			return false;
		};
		plugin->GetRenderTextureSize = [](void* objects, const char* name, int& w, int& h) {
			ObjectManager* obj = (ObjectManager*)objects;
			ObjectManagerItem* item = obj->Get(name);
			glm::ivec2 tsize = obj->GetRenderTextureSize(item);
			w = tsize.x;
			h = tsize.y;
		};
		plugin->GetDepthTexture = [](void* objects, const char* name) -> unsigned int {
			ObjectManager* obj = (ObjectManager*)objects;
			return obj->Get(name)->RT->DepthStencilBuffer;
		};
		plugin->ScaleSize = [](float size) -> float {
			return Settings::Instance().CalculateSize(size);
		};

		if (plugin->GetVersion() >= 2) {
			ed::IPlugin2* plugin2 = (ed::IPlugin2*)plugin;
			plugin2->GetHostIPluginMaxVersion = []() -> int {
				return 2;
			};
			plugin2->ImGuiFileDialogOpen = [](const char* key, const char* title, const char* filter) {
				ifd::FileDialog::Instance().Save(key, title, filter);
			};
			plugin2->ImGuiDirectoryDialogOpen = [](const char* key, const char* title) {
				ifd::FileDialog::Instance().Open(key, title, "");
			};
			plugin2->ImGuiFileDialogIsDone = [](const char* key) -> bool {
				return ifd::FileDialog::Instance().IsDone(key);
			};
			plugin2->ImGuiFileDialogClose = [](const char* key) {
				ifd::FileDialog::Instance().Close();
			};
			plugin2->ImGuiFileDialogGetResult = []() -> bool {
				return ifd::FileDialog::Instance().HasResult();
			};
			plugin2->ImGuiFileDialogGetPath = [](char* outPath) {
				std::string res = ifd::FileDialog::Instance().GetResult().u8string();
				strcpy(outPath, res.c_str());
			};
			plugin2->DebuggerImmediate = [](void* Debugger, const char* expr) -> const char* {
				static std::string buffer;
				spvm_result_t resType = nullptr;
				spvm_result_t res = ((ed::DebugInformation*)Debugger)->Immediate(expr, resType);

				if (res != nullptr) {
					std::stringstream ss;
					((ed::DebugInformation*)Debugger)->GetVariableValueAsString(ss, ((ed::DebugInformation*)Debugger)->GetVMImmediate(), resType, res->members, res->member_count, "");
					buffer = ss.str();
				} else
					buffer = "ERROR";

				return buffer.c_str();
			};
		}

		if (plugin->GetVersion() >= 3) {
			ed::IPlugin3* plugin3 = (ed::IPlugin3*)plugin;
			plugin3->RegisterPlugin = [](void* pluginManager, void* plugin, const char* pname, int apiVer, int pluginVer, void* procDLL) {
				PluginManager* pm = (PluginManager*)pluginManager;
				IPlugin1* p = (IPlugin1*)plugin;
				pm->RegisterPlugin(p, pname, apiVer, pluginVer, procDLL);
			};
			plugin3->GetEditorPipelineItem = nullptr;
			plugin3->SetViewportSize = [](float width, float height) {
				SystemVariableManager::Instance().SetViewportSize(width, height);
			};
			plugin3->IsObjectBound = [](void* Objects, const char* name, void* pipelineItem) -> int {
				ObjectManager* obj = (ObjectManager*)Objects;
				return obj->IsBound(obj->Get(name), (PipelineItem*)pipelineItem);
			};
			plugin3->DebuggerStepIntoPluginEditor = [](void* Debugger, void* UI, void* Plugin, int lang, int editorID) {
				((ed::DebugInformation*)Debugger)->StepInto();
				int curLine = ((ed::DebugInformation*)Debugger)->GetCurrentLine();
				((IPlugin3*)Plugin)->ShaderEditor_SetLineIndicator(lang, editorID, curLine);
			};
			plugin3->DebuggerGetVariableValue = [](void* Debugger, const char* name, char* value, int valueLength) {
				ed::DebugInformation* dbgr = ((ed::DebugInformation*)Debugger);

				spvm_result_t memType;
				size_t memCount = 0;
				spvm_member_t mem = dbgr->GetVariable(name, memCount, memType);

				std::stringstream ss;
				dbgr->GetVariableValueAsString(ss, dbgr->GetVM(), memType, mem, memCount, "");

				std::string res = ss.str();
				if (res.size() >= valueLength - 1)
					res = res.substr(0, valueLength - 1);

				strcpy(value, res.c_str());
			};
			plugin3->DebuggerStopPluginEditor = [](void* Debugger, void* UI, void* Plugin, int lang, int editorID) {
				((ed::DebugInformation*)Debugger)->SetDebugging(false);
				((IPlugin3*)Plugin)->ShaderEditor_SetLineIndicator(lang, editorID, -1);
			};
			plugin3->DebuggerIsVMRunning = [](void* Debugger) -> bool {
				return ((ed::DebugInformation*)Debugger)->IsVMRunning();
			};
		}

#ifdef SHADERED_DESKTOP
		bool initResult = plugin->Init(false, SHADERED_VERSION);
#else
		bool initResult = plugin->Init(true, SHADERED_VERSION);
#endif
		plugin->InitUI(ImGui::GetCurrentContext());

		if (initResult)
			ed::Logger::Get().Log("Plugin \"" + pname + "\" successfully initialized.");
		else {
			ed::Logger::Get().Log("Failed to initialize plugin \"" + pname + "\".");
		
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
			DestroyPluginFn fnDestroyPlugin = (DestroyPluginFn)dlsym(procDLL, "DestroyPlugin");
			if (fnDestroyPlugin)
				(*fnDestroyPlugin)(plugin);

			dlclose(procDLL);
#else
			DestroyPluginFn fnDestroyPlugin = (DestroyPluginFn)GetProcAddress((HINSTANCE)procDLL, "DestroyPlugin");
			if (fnDestroyPlugin)
				(*fnDestroyPlugin)(plugin);

			FreeLibrary((HINSTANCE)procDLL);
#endif
			return;
		}

		m_plugins.push_back(plugin);
		m_proc.push_back(procDLL);
		m_names.push_back(pname);
		m_apiVersion.push_back(apiVer);
		m_pluginVersion.push_back(pluginVer);

		bool isIn = false;
		for (const auto& line : m_iniLines) {
			if (isIn) {
				size_t sepLoc = line.find('=');
				if (sepLoc != std::string::npos) {
					std::string key = line.substr(0, sepLoc);
					std::string val = line.substr(sepLoc + 1);

					plugin->Options_Parse(key.c_str(), val.c_str());
				}
			}

			if (line.find("[" + pname + "]") == 0)
				isIn = true;
			else if (line.size() > 0 && line[0] == '[')
				isIn = false;
		}

		int customLangCount = plugin->CustomLanguage_GetCount();
		for (int i = 0; i < customLangCount; i++) {
			std::string langExt(plugin->CustomLanguage_GetDefaultExtension(i));
			bool isUsedSomewhere = false;

			for (const std::string& ext : Settings::Instance().General.HLSLExtensions) {
				if (ext == langExt) {
					isUsedSomewhere = true;
					break;
				}
			}
			for (const std::string& ext : Settings::Instance().General.VulkanGLSLExtensions) {
				if (ext == langExt) {
					isUsedSomewhere = true;
					break;
				}
			}
			for (const auto& pair : Settings::Instance().General.PluginShaderExtensions) {
				for (const std::string& ext : pair.second) {
					if (ext == langExt) {
						isUsedSomewhere = true;
						break;
					}
				}
			}

			if (!isUsedSomewhere) {
				std::vector<std::string>& myExts = Settings::Instance().General.PluginShaderExtensions[plugin->CustomLanguage_GetName(i)];
				if (myExts.size() == 0) myExts.push_back(langExt);
			}
		}

		if (plugin->GetVersion() >= 3) {
			ed::IPlugin3* plugin3 = (ed::IPlugin3*)plugin;
			plugin3->PluginManager_RegisterPlugins();
		}
	}

	IPlugin1* PluginManager::GetPlugin(const std::string& plugin)
	{
		std::string pluginLower = plugin;
		std::transform(pluginLower.begin(), pluginLower.end(), pluginLower.begin(), ::tolower);

		for (int i = 0; i < m_names.size(); i++) {
			std::string nameLower = m_names[i];
			std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

			if (nameLower == pluginLower)
				return m_plugins[i];
		}

		return nullptr;
	}
	const std::string& PluginManager::GetPluginName(IPlugin1* plugin)
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

	bool PluginManager::IsLoaded(const std::string& plugin)
	{
		for (const auto& pl : m_names)
			if (pl == plugin)
				return true;

		return false;
	}

	void PluginManager::HandleDropFile(const char* filename)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_plugins[i]->HandleDropFile(filename))
				break;
	}
	void PluginManager::HandleApplicationEvent(plugin::ApplicationEvent event, void* data1, void* data2)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			m_plugins[i]->HandleApplicationEvent(event, data1, data2);
	}

	void PluginManager::ShowContextItems(const std::string& menu, void* owner, void* extraData)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_plugins[i]->HasContextItems(menu.c_str())) {
				ImGui::Separator();
				m_plugins[i]->ShowContextItems(menu.c_str(), owner, extraData);
			}
	}
	void PluginManager::ShowContextItems(IPlugin1* plugin, const std::string& menu, void* owner)
	{
		if (plugin->HasContextItems(menu.c_str())) {
			ImGui::Separator();
			plugin->ShowContextItems(menu.c_str(), owner);
		}
	}
	void PluginManager::ShowMenuItems(const std::string& menu)
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_plugins[i]->HasMenuItems(menu.c_str())) {
				ImGui::Separator();
				m_plugins[i]->ShowMenuItems(menu.c_str());
			}
	}
	void PluginManager::ShowCustomMenu()
	{
		for (int i = 0; i < m_plugins.size(); i++)
			if (m_plugins[i]->HasCustomMenuItem())
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
			if (m_plugins[i]->Options_HasSection()) {
				std::string pluginName = m_names[i];
				std::transform(pluginName.begin(), pluginName.end(), pluginName.begin(), tolower);
				if (qLower.empty() || pluginName.find(qLower) != std::string::npos) {
					if (ImGui::CollapsingHeader(m_names[i].c_str(), ImGuiTreeNodeFlags_DefaultOpen))
						m_plugins[i]->Options_RenderSection();
				}
			}
		}
	}
	bool PluginManager::ShowSystemVariables(PluginSystemVariableData* data, ShaderVariable::ValueType type)
	{
		bool ret = false;
		for (int i = 0; i < m_plugins.size(); i++) {
			int nameCount = m_plugins[i]->SystemVariables_GetNameCount((plugin::VariableType)type);
			for (int j = 0; j < nameCount; j++) {
				const char* name = m_plugins[i]->SystemVariables_GetName((plugin::VariableType)type, j);
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
		for (int i = 0; i < m_plugins.size(); i++) {
			int nameCount = m_plugins[i]->VariableFunctions_GetNameCount((plugin::VariableType)type);
			
			for (int j = 0; j < nameCount; j++) {
				const char* name = m_plugins[i]->VariableFunctions_GetName((plugin::VariableType)type, j);
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