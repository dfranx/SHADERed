#include "GUIManager.h"
#include "InterfaceManager.h"
#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_opengl3.h>
#include <imgui/examples/imgui_impl_sdl.h>
#include "UI/CreateItemUI.h"
#include "UI/CodeEditorUI.h"
#include "UI/ObjectListUI.h"
#include "UI/MessageOutputUI.h"
#include "UI/PipelineUI.h"
#include "UI/PropertyUI.h"
#include "UI/PreviewUI.h"
#include "UI/OptionsUI.h"
#include "UI/PinnedUI.h"
#include "UI/UIHelper.h"
#include "Objects/Logger.h"
#include "Objects/Names.h"
#include "Objects/Settings.h"
#include "Objects/ThemeContainer.h"
#include "Objects/KeyboardShortcuts.h"
#include "Objects/FunctionVariableManager.h"

#include <fstream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <ghc/filesystem.hpp>

#if defined(__APPLE__)
	// no includes on mac os
#elif defined(__linux__) || defined(__unix__)
	// no includes on linux
#elif defined(_WIN32)
	#include <windows.h>
#endif

#define HARRAYSIZE(a) (sizeof(a)/sizeof(*a))

namespace ed
{
	GUIManager::GUIManager(ed::InterfaceManager* objects, SDL_Window* wnd, SDL_GLContext* gl)
	{
		m_data = objects;
		m_wnd = wnd;
		m_gl  = gl;
		m_settingsBkp = new Settings();
		m_isCreateRTOpened = false;
		m_isCreateItemPopupOpened = false;
		m_previewSaveSize = glm::ivec2(1920, 1080);
		m_savePreviewPopupOpened = false;
		m_optGroup = 0;
		m_optionsOpened = false;
		m_cachedFont = "null";
		m_cachedFontSize = 15;
		m_performanceMode = false;
		m_perfModeFake = false;
		m_fontNeedsUpdate = false;
		m_isCreateItemPopupOpened = false;
		m_isCreateCubemapOpened = false;
		m_isCreateRTOpened = false;
		m_isNewProjectPopupOpened = false;
		m_isAboutOpen = false;

		Settings::Instance().Load();
		m_loadTemplateList();
		
		Logger::Get().Log("Initializing Dear ImGUI");
		
		// set vsync on startup
		SDL_GL_SetSwapInterval(Settings::Instance().General.VSync);

		// Initialize imgui
		ImGui::CreateContext();
		
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontDefault();
		io.IniFilename = IMGUI_INI_FILE;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_DockingEnable /*| ImGuiConfigFlags_ViewportsEnable TODO: allow this on windows? test on linux?*/;
		io.ConfigDockingWithShift = false;

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowMenuButtonPosition = ImGuiDir_Right;

		ImGui_ImplOpenGL3_Init(SDL_GLSL_VERSION);
		ImGui_ImplSDL2_InitForOpenGL(m_wnd, *m_gl);

		ImGui::StyleColorsDark();

		Logger::Get().Log("Creating various UI view objects");

		m_views.push_back(new PinnedUI(this, objects, "Pinned"));
		m_views.push_back(new PreviewUI(this, objects, "Preview"));
		m_views.push_back(new CodeEditorUI(this, objects, "Code"));
		m_views.push_back(new MessageOutputUI(this, objects, "Output"));
		m_views.push_back(new ObjectListUI(this, objects, "Objects"));
		m_views.push_back(new PipelineUI(this, objects, "Pipeline"));
		m_views.push_back(new PropertyUI(this, objects, "Properties"));

		KeyboardShortcuts::Instance().Load();
		m_setupShortcuts();

		m_options = new OptionsUI(this, objects, "Options");
		m_createUI = new CreateItemUI(this, objects);

		// turn on the tracker on startup
		((CodeEditorUI*)Get(ViewID::Code))->SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange);

		((OptionsUI*)m_options)->SetGroup(OptionsUI::Page::General);


		// get dpi
		float dpi;
		int wndDisplayIndex = SDL_GetWindowDisplayIndex(wnd);
		SDL_GetDisplayDPI(wndDisplayIndex, NULL, &dpi, NULL);
		dpi /= 96.0f;
		
		// enable dpi awareness		
		if (Settings::Instance().General.AutoScale) {
			Settings::Instance().DPIScale = dpi;
			Logger::Get().Log("Setting DPI to " + std::to_string(dpi));
		}
		m_cacheUIScale = Settings::Instance().TempScale = Settings::Instance().DPIScale;

		ImGui::GetStyle().ScaleAllSizes(Settings::Instance().DPIScale);
	}
	GUIManager::~GUIManager()
	{
		// turn off the tracker on exit
		((CodeEditorUI*)Get(ViewID::Code))->SetTrackFileChanges(false);

		Logger::Get().Log("Shutting down UI");

		for (auto view : m_views)
			delete view;

		delete m_createUI;
		delete m_settingsBkp;

		ImGui_ImplSDL2_Shutdown();
		ImGui_ImplOpenGL3_Shutdown();
		ImGui::DestroyContext();
	}

	void GUIManager::OnEvent(const SDL_Event& e)
	{
		m_imguiHandleEvent(e);

		// check for shortcut presses
		if (e.type == SDL_KEYDOWN) {
			if (!(m_optionsOpened && ((OptionsUI*)m_options)->IsListening())) {
				bool codeHasFocus = ((CodeEditorUI*)Get(ViewID::Code))->HasFocus();

				if (!(!codeHasFocus && ImGui::GetIO().WantTextInput))
					KeyboardShortcuts::Instance().Check(e, codeHasFocus);
			}
		}

		if (m_optionsOpened)
			m_options->OnEvent(e);

		for (auto view : m_views)
			view->OnEvent(e);
	}
	void GUIManager::Update(float delta)
	{
		Settings& settings = Settings::Instance();
		m_performanceMode = m_perfModeFake;

		// update audio textures
		m_data->Objects.Update(delta);
		FunctionVariableManager::ClearVariableList();

		// update editor & workspace font
		if (((CodeEditorUI*)Get(ViewID::Code))->NeedsFontUpdate() ||
			((m_cachedFont != settings.General.Font ||
				m_cachedFontSize != settings.General.FontSize) &&
				strcmp(settings.General.Font, "null") != 0) ||
			m_fontNeedsUpdate)
		{
			Logger::Get().Log("Updating fonts...");

			std::pair<std::string, int> edFont = ((CodeEditorUI*)Get(ViewID::Code))->GetFont();

			m_cachedFont = settings.General.Font;
			m_cachedFontSize = settings.General.FontSize;
			m_fontNeedsUpdate = false;

			ImFontAtlas* fonts = ImGui::GetIO().Fonts;
			fonts->Clear();

			ImFont* font = nullptr;
			font = fonts->AddFontFromFileTTF(m_cachedFont.c_str(), m_cachedFontSize * Settings::Instance().DPIScale);

			ImFont* edFontPtr = fonts->AddFontFromFileTTF(edFont.first.c_str(), edFont.second * Settings::Instance().DPIScale);

			if (font == nullptr || edFontPtr == nullptr) {
				fonts->Clear();
				font = fonts->AddFontDefault();
				font = fonts->AddFontDefault();

				Logger::Get().Log("Failed to load fonts", true);
			}

			ImGui::GetIO().FontDefault = font;
			fonts->Build();

			ImGui_ImplOpenGL3_DestroyFontsTexture();

			((CodeEditorUI*)Get(ViewID::Code))->UpdateFont();
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_wnd);
		ImGui::NewFrame();

		// create a fullscreen imgui panel that will host a dockspace
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpaceWnd", 0, window_flags);
		ImGui::PopStyleVar(3);

		// DockSpace
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable && !m_performanceMode)
		{
			ImGuiID dockspace_id = ImGui::GetID("DockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
		}

		// rebuild
		if (((CodeEditorUI*)Get(ViewID::Code))->TrackedFilesNeedUpdate()) {
			std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
			for (PipelineItem*& pass : passes)
				m_data->Renderer.Recompile(pass->Name);
			((CodeEditorUI*)Get(ViewID::Code))->EmptyTrackedFiles();
		}


		// menu
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::BeginMenu("New")) {
					if (ImGui::MenuItem("Empty")) {
						m_selectedTemplate = "?empty";
						m_isNewProjectPopupOpened = true;
					}
					ImGui::Separator();

					for (int i = 0; i < m_templates.size(); i++)
						if (ImGui::MenuItem(m_templates[i].c_str())) {
							m_selectedTemplate = m_templates[i];
							m_isNewProjectPopupOpened = true;
						}
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Create shader file")) {
					std::string file;
					bool success = UIHelper::GetSaveFileDialog(file, "hlsl;glsl;vert;frag;geom");
					
					if (success) {
						// create a file (cross platform)
						std::ofstream ofs(file);
						ofs << "// empty shader file\n"; 
						ofs.close();
					}
				}
				if (ImGui::MenuItem("Open", KeyboardShortcuts::Instance().GetString("Project.Open").c_str())) {
					std::string file;
					bool success = UIHelper::GetOpenFileDialog(file, "sprj");

					if (success)
						Open(file);
				}
				if (ImGui::MenuItem("Save", KeyboardShortcuts::Instance().GetString("Project.Save").c_str())) {
					if (m_data->Parser.GetOpenedFile() == "")
						SaveAsProject();
					else
						m_data->Parser.Save();
				}
				if (ImGui::MenuItem("Save As", KeyboardShortcuts::Instance().GetString("Project.SaveAs").c_str()))
					SaveAsProject();
				if (ImGui::MenuItem("Save Preview as Image", KeyboardShortcuts::Instance().GetString("Preview.SaveImage").c_str()))
					m_savePreviewPopupOpened = true;
				if (ImGui::MenuItem("Open project directory")) {
					std::string prpath = m_data->Parser.GetProjectPath("");
					#if defined(__APPLE__)
						system(("open " + prpath).c_str()); // [MACOS]
					#elif defined(__linux__) || defined(__unix__)
						system(("xdg-open " + prpath).c_str());
					#elif defined(_WIN32)
						ShellExecuteA(NULL, "open", prpath.c_str(), NULL, NULL, SW_SHOWNORMAL);
					#endif
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Exit", KeyboardShortcuts::Instance().GetString("Window.Exit").c_str())) {
					SDL_DestroyWindow(m_wnd);
					return;
				}

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Project")) {
				if (ImGui::MenuItem("Rebuild project", KeyboardShortcuts::Instance().GetString("Project.Rebuild").c_str())) {
					((CodeEditorUI*)Get(ViewID::Code))->SaveAll();

					std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
					for (PipelineItem*& pass : passes)
						m_data->Renderer.Recompile(pass->Name);
				}
				if (ImGui::MenuItem("Render", KeyboardShortcuts::Instance().GetString("Preview.SaveImage").c_str()))
					m_savePreviewPopupOpened = true;
				if (ImGui::BeginMenu("Create")) {
					if (ImGui::MenuItem("Pass", KeyboardShortcuts::Instance().GetString("Project.NewShaderPass").c_str()))
						this->CreateNewShaderPass();
					if (ImGui::MenuItem("Texture", KeyboardShortcuts::Instance().GetString("Project.NewTexture").c_str()))
						this->CreateNewTexture();
					if (ImGui::MenuItem("Cubemap", KeyboardShortcuts::Instance().GetString("Project.NewCubeMap").c_str()))
						this->CreateNewCubemap();
					if (ImGui::MenuItem("Audio", KeyboardShortcuts::Instance().GetString("Project.NewAudio").c_str()))
						this->CreateNewAudio();
					if (ImGui::MenuItem("Render Texture", KeyboardShortcuts::Instance().GetString("Project.NewRenderTexture").c_str()))
						this->CreateNewRenderTexture();
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Window")) {
				for (auto view : m_views) {
					if (view->Name != "Code") // dont show the "Code" UI view in this menu
						ImGui::MenuItem(view->Name.c_str(), 0, &view->Visible);
				}
				ImGui::Separator();
				
				ImGui::MenuItem("Performance Mode", KeyboardShortcuts::Instance().GetString("Workspace.PerformanceMode").c_str(), &m_perfModeFake);

				if (ImGui::MenuItem("Options", KeyboardShortcuts::Instance().GetString("Workspace.Options").c_str())) {
					m_optionsOpened = true;
					*m_settingsBkp = settings;
					m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
				}

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help")) {
				if (ImGui::BeginMenu("Support Us")) {
					if (ImGui::MenuItem("Patreon")) { 
						#if defined(__APPLE__)
							system("open https://www.patreon.com/dfranx"); // [MACOS]
						#elif defined(__linux__) || defined(__unix__)
							system("xdg-open https://www.patreon.com/dfranx");
						#elif defined(_WIN32)
							ShellExecuteW(NULL, L"open", L"https://www.patreon.com/dfranx", NULL, NULL, SW_SHOWNORMAL);
						#endif
						}
					if (ImGui::MenuItem("PayPal")) { 
						#if defined(__APPLE__)
							system("open https://www.paypal.me/dfranx"); // [MACOS]
						#elif defined(__linux__) || defined(__unix__)
							system("xdg-open https://www.paypal.me/dfranx");
						#elif defined(_WIN32)
							ShellExecuteW(NULL, L"open", L"https://www.paypal.me/dfranx", NULL, NULL, SW_SHOWNORMAL);
						#endif
					}
					ImGui::EndMenu();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Tutorial")) {
					#if defined(__APPLE__)
						system("open https://github.com/dfranx/SHADERed/blob/master/TUTORIAL.md"); // [MACOS]
					#elif defined(__linux__) || defined(__unix__)
						system("xdg-open https://github.com/dfranx/SHADERed/blob/master/TUTORIAL.md");
					#elif defined(_WIN32)
						ShellExecuteW(NULL, L"open", L"https://github.com/dfranx/SHADERed/blob/master/TUTORIAL.md", NULL, NULL, SW_SHOWNORMAL);
					#endif
				}
				if (ImGui::MenuItem("Send feedback")) {
					#if defined(__APPLE__)
						system("open https://www.github.com/dfranx/SHADERed/issues"); // [MACOS]
					#elif defined(__linux__) || defined(__unix__)
						system("xdg-open https://www.github.com/dfranx/SHADERed/issues");
					#elif defined(_WIN32)
						ShellExecuteW(NULL, L"open", L"https://www.github.com/dfranx/SHADERed/issues", NULL, NULL, SW_SHOWNORMAL);
					#endif
				}
				if (ImGui::MenuItem("About SHADERed")) { m_isAboutOpen = true;}

				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		if (m_performanceMode)
			((PreviewUI*)Get(ViewID::Preview))->Update(delta);

		ImGui::End();

		int wndW, wndH;
		SDL_GetWindowSize(m_wnd, &wndW, &wndH);

		if (!m_performanceMode) {
			for (auto view : m_views)
				if (view->Visible) {
					ImGui::SetNextWindowSizeConstraints(ImVec2(80, 80), ImVec2(wndW, wndH));
					if (ImGui::Begin(view->Name.c_str(), &view->Visible)) view->Update(delta);
					ImGui::End();
				}

			Get(ViewID::Code)->Update(delta);
		}

		// handle the "build occured" event
		if (settings.General.AutoOpenErrorWindow && m_data->Messages.BuildOccured) {
			size_t errors = m_data->Messages.GetMessages().size();
			if (errors > 0 && !Get(ViewID::Output)->Visible)
				Get(ViewID::Output)->Visible = true;
			m_data->Messages.BuildOccured = false;
		}

		// render options window
		if (m_optionsOpened) {
			ImGui::Begin("Options", &m_optionsOpened, ImGuiWindowFlags_NoDocking);
			m_renderOptions();
			ImGui::End();
		}

		// open popup for creating items
		if (m_isCreateItemPopupOpened) {
			ImGui::OpenPopup("Create Item##main_create_item");
			m_isCreateItemPopupOpened = false;
		}

		// open popup for saving preview as image
		if (m_savePreviewPopupOpened) {
			ImGui::OpenPopup("Save Preview##main_save_preview");
			m_previewSavePath = "render.png";
			m_savePreviewPopupOpened = false;
		}

		// open popup for creating cubemap
		if (m_isCreateCubemapOpened) {
			ImGui::OpenPopup("Create cubemap##main_create_cubemap");
			m_isCreateCubemapOpened = false;
		}

		// open popup for creating render texture
		if (m_isCreateRTOpened) {
			ImGui::OpenPopup("Create RT##main_create_rt");
			m_isCreateRTOpened = false;
		}

		// open popup for openning new project
		if (m_isNewProjectPopupOpened) {
			ImGui::OpenPopup("Are you sure?##main_new_proj");
			m_isNewProjectPopupOpened = false;
		}

		// open about popup
		if (m_isAboutOpen) {
			ImGui::OpenPopup("About##main_about");
			m_isAboutOpen = false;
		}

		// Create Item popup
		ImGui::SetNextWindowSize(ImVec2(430 * Settings::Instance().DPIScale, 175 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Create Item##main_create_item")) {
			m_createUI->Update(delta);

			if (ImGui::Button("Ok")) {
				if (m_createUI->Create())
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create cubemap popup
		ImGui::SetNextWindowSize(ImVec2(430 * Settings::Instance().DPIScale, 275 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Create cubemap##main_create_cubemap")) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 64);

			static std::string left, top, front, bottom, right, back;

			ImGui::Text("Left: %s", left.c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 60);
			if (ImGui::Button("Change##left")) {
				UIHelper::GetOpenFileDialog(left, "png;bmp;jpg,jpeg;tga");
				left = m_data->Parser.GetRelativePath(left);
			}

			ImGui::Text("Top: %s", top.c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 60);
			if (ImGui::Button("Change##top")) {
				UIHelper::GetOpenFileDialog(top, "png;bmp;jpg,jpeg;tga");
				top = m_data->Parser.GetRelativePath(top);
			}

			ImGui::Text("Front: %s", front.c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 60);
			if (ImGui::Button("Change##front")) {
				UIHelper::GetOpenFileDialog(front, "png;bmp;jpg,jpeg;tga");
				front = m_data->Parser.GetRelativePath(front);
			}

			ImGui::Text("Bottom: %s", bottom.c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 60);
			if (ImGui::Button("Change##bottom")) {
				UIHelper::GetOpenFileDialog(bottom, "png;bmp;jpg,jpeg;tga");
				bottom = m_data->Parser.GetRelativePath(bottom);
			}

			ImGui::Text("Right: %s", right.c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 60);
			if (ImGui::Button("Change##right")) {
				UIHelper::GetOpenFileDialog(right, "png;bmp;jpg,jpeg;tga");
				right = m_data->Parser.GetRelativePath(right);
			}

			ImGui::Text("Back: %s", back.c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 60);
			if (ImGui::Button("Change##back")) {
				UIHelper::GetOpenFileDialog(back, "png;bmp;jpg,jpeg;tga");
				back = m_data->Parser.GetRelativePath(back);
			}


			if (ImGui::Button("Ok") && strlen(buf) > 0 && !m_data->Objects.Exists(buf)) {
				if (m_data->Objects.CreateCubemap(buf, left, top, front, bottom, right, back))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create RT popup
		ImGui::SetNextWindowSize(ImVec2(430 * Settings::Instance().DPIScale, 175 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Create RT##main_create_rt")) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 64);

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateRenderTexture(buf))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create about popup
		ImGui::SetNextWindowSize(ImVec2(270 * Settings::Instance().DPIScale, 150 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("About##main_about")) {
			ImGui::TextWrapped("(C) 2019 dfranx");
			ImGui::TextWrapped("Version 1.0");
			ImGui::NewLine();
			ImGui::TextWrapped("This app is open sourced: ");
			ImGui::SameLine();
			if (ImGui::Button("link")) {
				#if defined(__APPLE__)
					system("open https://www.github.com/dfranx/SHADERed"); // [MACOS]
				#elif defined(__linux__) || defined(__unix__)
					system("xdg-open https://www.github.com/dfranx/SHADERed");
				#elif defined(_WIN32)
					ShellExecuteW(NULL, L"open", L"https://www.github.com/dfranx/SHADERed", NULL, NULL, SW_SHOWNORMAL);
				#endif
			}

			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create new project
		ImGui::SetNextWindowSize(ImVec2(300 * Settings::Instance().DPIScale, 100 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Are you sure?##main_new_proj")) {
			ImGui::TextWrapped("You will lose your unsaved progress if you create new project");
			if (ImGui::Button("Yes")) {
				std::string oldFile = m_data->Parser.GetOpenedFile();

				if (m_selectedTemplate == "?empty") {
						Settings::Instance().Project.FPCamera = false;
						Settings::Instance().Project.ClearColor = glm::vec4(0, 0, 0, 0);

						m_data->Renderer.FlushCache();
						((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
						((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
						((PreviewUI*)Get(ViewID::Preview))->Pick(nullptr);
						((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
						((PipelineUI*)Get(ViewID::Pipeline))->Reset();
						m_data->Pipeline.New(false);

						SDL_SetWindowTitle(m_wnd, "SHADERed");
				} else {
					m_data->Parser.SetTemplate(m_selectedTemplate);

					m_data->Renderer.FlushCache();
					((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
					((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
					((PreviewUI*)Get(ViewID::Preview))->Pick(nullptr);
					((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
					((PipelineUI*)Get(ViewID::Pipeline))->Reset();
					m_data->Pipeline.New();

					m_data->Parser.SetTemplate(settings.General.StartUpTemplate);

					SDL_SetWindowTitle(m_wnd, ("SHADERed (" + m_selectedTemplate + ")").c_str());
				}

				bool chosen = SaveAsProject();
				if (!chosen) {
					if (oldFile != "") {
						m_data->Renderer.FlushCache();
						((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
						((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
						((PreviewUI*)Get(ViewID::Preview))->Pick(nullptr);
						((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
						((PipelineUI*)Get(ViewID::Pipeline))->Reset();
						m_data->Parser.Open(oldFile);

					}
					else
						m_data->Parser.OpenTemplate();
				}

				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Save preview
		ImGui::SetNextWindowSize(ImVec2(400, 145), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Save Preview##main_save_preview")) {
			ImGui::TextWrapped("Path: %s", m_previewSavePath.c_str());
			ImGui::SameLine();
			if (ImGui::Button("...##save_prev_path"))
				UIHelper::GetSaveFileDialog(m_previewSavePath, "png;jpg,jpeg;bmp;tga");
			
			ImGui::Text("Width: ");
			ImGui::SameLine();
			ImGui::Indent(55);
			ImGui::InputInt("##save_prev_sizew", &m_previewSaveSize.x);
			ImGui::Unindent(55);

			ImGui::Text("Height: ");
			ImGui::SameLine();
			ImGui::Indent(55);
			ImGui::InputInt("##save_prev_sizeh", &m_previewSaveSize.y);
			ImGui::Unindent(55);

			if (ImGui::Button("Save")) {
				if (m_previewSaveSize.x > 0 && m_previewSaveSize.y > 0)
					m_data->Renderer.Render(m_previewSaveSize.x, m_previewSaveSize.y);
				
				GLuint tex = m_data->Renderer.GetTexture();
				glBindTexture(GL_TEXTURE_2D, tex);
				unsigned char *pixels = (unsigned char*)malloc(m_previewSaveSize.x * m_previewSaveSize.y * 4);
				glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
				glBindTexture(GL_TEXTURE_2D, 0);

				std::string ext = m_previewSavePath.substr(m_previewSavePath.find_last_of('.')+1);
				
				if (ext == "jpg" || ext == "jpeg")
					stbi_write_jpg(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, pixels, 100);
				else if (ext == "bmp")
					stbi_write_bmp(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, pixels);
				else if (ext == "tga")
					stbi_write_tga(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, pixels);
				else
					stbi_write_png(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, pixels, m_previewSaveSize.x * 4);

				free(pixels);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// render ImGUI
		ImGui::Render();
	}
	void GUIManager::m_renderOptions()
	{
		OptionsUI* options = (OptionsUI*)m_options;
		static const char* optGroups[5] = {
			"General",
			"Editor",
			"Shortcuts",
			"Preview",
			"Project"
		};

		float height = abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y - ImGui::GetStyle().WindowPadding.y*2) / ImGui::GetTextLineHeightWithSpacing() - 1;
		
		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, 100 * Settings::Instance().DPIScale + ImGui::GetStyle().WindowPadding.x * 2);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(100 * Settings::Instance().DPIScale);
		if (ImGui::ListBox("##optiongroups", &m_optGroup, optGroups, HARRAYSIZE(optGroups), height))
			options->SetGroup((OptionsUI::Page)m_optGroup);
		ImGui::PopStyleColor();

		ImGui::NextColumn();

		options->Update(0.0f);

		ImGui::Columns();

		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 160 * Settings::Instance().DPIScale);
		if (ImGui::Button("OK", ImVec2(70 * Settings::Instance().DPIScale, 0))) {
			Settings::Instance().Save();
			KeyboardShortcuts::Instance().Save();

			CodeEditorUI* code = ((CodeEditorUI*)Get(ViewID::Code));

			code->SetTabSize(Settings::Instance().Editor.TabSize);
			code->SetInsertSpaces(Settings::Instance().Editor.InsertSpaces);
			code->SetSmartIndent(Settings::Instance().Editor.SmartIndent);
			code->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
			code->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
			code->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
			code->SetFont(Settings::Instance().Editor.Font, Settings::Instance().Editor.FontSize);
			code->SetHorizontalScrollbar(Settings::Instance().Editor.HorizontalScroll);
			code->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
			code->SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange);
			code->UpdateShortcuts();

			if (Settings::Instance().TempScale != m_cacheUIScale) {
				Settings::Instance().DPIScale = Settings::Instance().TempScale;
				ImGui::GetStyle().ScaleAllSizes(Settings::Instance().DPIScale / m_cacheUIScale);
				m_cacheUIScale = Settings::Instance().DPIScale;
				m_fontNeedsUpdate = true;
			}

			m_optionsOpened = false;
		}

		ImGui::SameLine();

		ImGui::SetCursorPosX(ImGui::GetWindowWidth()-80 * Settings::Instance().DPIScale);
		if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
			Settings::Instance() = *m_settingsBkp;
			KeyboardShortcuts::Instance().SetMap(m_shortcutsBkp);
			((OptionsUI*)m_options)->ApplyTheme();
			m_optionsOpened = false;
		}


	}
	void GUIManager::Render()
	{
		// actually render to back buffer
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Update and Render additional Platform Windows
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}
	UIView * GUIManager::Get(ViewID view)
	{
		if (view == ViewID::Options)
			return m_options;
		return m_views[(int)view];
	}
	void GUIManager::SaveSettings()
	{
		std::ofstream data("data/gui.dat");

		for (auto view : m_views)
			data.put(view->Visible);

		data.close();
	}
	void GUIManager::LoadSettings()
	{
		std::ifstream data("data/gui.dat");

		if (data.is_open()) {
			for (auto view : m_views)
				view->Visible = data.get();

			data.close();
		}

		Get(ViewID::Code)->Visible = false;

		((OptionsUI*)m_options)->ApplyTheme();
	}
	void GUIManager::m_imguiHandleEvent(const SDL_Event& e)
	{
		ImGui_ImplSDL2_ProcessEvent(&e);
	}
	bool GUIManager::SaveAsProject(bool restoreCached)
	{
		std::string file;
		bool success = UIHelper::GetSaveFileDialog(file, "sprj");

		if (success) {
			m_data->Parser.SaveAs(file, true);


			m_data->Renderer.FlushCache();

			// cache opened code editors
			CodeEditorUI* editor = ((CodeEditorUI*)Get(ViewID::Code));
			std::vector<std::pair<std::string, int>> files = editor->GetOpenedFiles();
			std::vector<std::string> filesData = editor->GetOpenedFilesData();

			// close all
			editor->CloseAll();
			((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
			((PreviewUI*)Get(ViewID::Preview))->Pick(nullptr);
			((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
			((PipelineUI*)Get(ViewID::Pipeline))->Reset();

			m_data->Parser.Open(file);

			std::string projName = m_data->Parser.GetOpenedFile();
			projName = projName.substr(projName.find_last_of("/\\") + 1);

			SDL_SetWindowTitle(m_wnd, ("SHADERed (" + projName + ")").c_str());

			// returned cached state
			if (restoreCached) {
				for (auto& file : files) {
					PipelineItem* item = m_data->Pipeline.Get(file.first.c_str());

					if (file.second == 0)
						editor->OpenVS(*item);
					else if (file.second == 1)
						editor->OpenPS(*item);
					else if (file.second == 2)
						editor->OpenGS(*item);
				}
				editor->SetOpenedFilesData(filesData);
			}
		}

		return success;
	}
	void GUIManager::Open(const std::string& file)
	{
		m_data->Renderer.FlushCache();

		((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
		((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
		((PreviewUI*)Get(ViewID::Preview))->Pick(nullptr);
		((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
		((PipelineUI*)Get(ViewID::Pipeline))->Reset();

		m_data->Parser.Open(file);

		std::string projName = m_data->Parser.GetOpenedFile();
		projName = projName.substr(projName.find_last_of("/\\") + 1);

		SDL_SetWindowTitle(m_wnd, ("SHADERed (" + projName + ")").c_str());
	}

	void GUIManager::CreateNewShaderPass() {
		m_createUI->SetType(PipelineItem::ItemType::ShaderPass);
		m_isCreateItemPopupOpened = true;
	}
	void GUIManager::CreateNewTexture() {
		std::string path;
		bool success = UIHelper::GetOpenFileDialog(path, "png;jpg;jpeg;bmp");
		
		if (!success)
			return;

		std::string file = m_data->Parser.GetRelativePath(path);
		if (!file.empty())
			m_data->Objects.CreateTexture(file);
	}
	void GUIManager::CreateNewAudio() {
		std::string path;
		bool success = UIHelper::GetOpenFileDialog(path, "wav;flac;ogg;midi");
		
		if (!success)
			return;

		std::string file = m_data->Parser.GetRelativePath(path);

		if (!file.empty())
			m_data->Objects.CreateAudio(file);
	}

	void GUIManager::m_setupShortcuts()
	{
		// PROJECT
		KeyboardShortcuts::Instance().SetCallback("Project.Rebuild", [=]() {
			((CodeEditorUI*)Get(ViewID::Code))->SaveAll();

			std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
			for (PipelineItem*& pass : passes)
				m_data->Renderer.Recompile(pass->Name);
		});
		KeyboardShortcuts::Instance().SetCallback("Project.Save", [=]() {
			if (m_data->Parser.GetOpenedFile() == "")
				SaveAsProject();
			else
				m_data->Parser.Save();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.SaveAs", [=]() {
			SaveAsProject();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.Open", [=]() {
			m_data->Renderer.FlushCache();
			std::string file;
			bool success = UIHelper::GetOpenFileDialog(file, "sprj");

			if (success) {
				((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
				((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
				((PreviewUI*)Get(ViewID::Preview))->Pick(nullptr);
				((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
				((PipelineUI*)Get(ViewID::Pipeline))->Reset();

				m_data->Parser.Open(file);
			}
		});
		KeyboardShortcuts::Instance().SetCallback("Project.New", [=]() {
			m_data->Renderer.FlushCache();
			((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
			((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
			((PreviewUI*)Get(ViewID::Preview))->Pick(nullptr);
			((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
			((PipelineUI*)Get(ViewID::Pipeline))->Reset();
			m_data->Pipeline.New();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewRenderTexture", [=]() {
			CreateNewRenderTexture();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewCubeMap", [=]() {
			CreateNewCubemap();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewTexture", [=]() {
			CreateNewTexture();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewAudio", [=]() {
			CreateNewAudio();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewShaderPass", [=]() {
			CreateNewShaderPass();
		});


		// PREVIEW
		KeyboardShortcuts::Instance().SetCallback("Preview.ToggleStatusbar", [=]() {
			Settings::Instance().Preview.StatusBar = !Settings::Instance().Preview.StatusBar;
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.SaveImage", [=]() {
			m_savePreviewPopupOpened = true;
		});

		// WORKSPACE
		KeyboardShortcuts::Instance().SetCallback("Workspace.PerformanceMode", [=]() {
			m_performanceMode = !m_performanceMode;
			m_perfModeFake = m_performanceMode;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HideOutput", [=]() {
			Get(ViewID::Output)->Visible = !Get(ViewID::Output)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HideEditor", [=]() {
			Get(ViewID::Code)->Visible = !Get(ViewID::Code)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HidePreview", [=]() {
			Get(ViewID::Preview)->Visible = !Get(ViewID::Preview)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HidePipeline", [=]() {
			Get(ViewID::Pipeline)->Visible = !Get(ViewID::Pipeline)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HidePinned", [=]() {
			Get(ViewID::Pinned)->Visible = !Get(ViewID::Pinned)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HideProperties", [=]() {
			Get(ViewID::Properties)->Visible = !Get(ViewID::Properties)->Visible;
			});
		KeyboardShortcuts::Instance().SetCallback("Workspace.Help", [=]() {
			#if defined(__APPLE__)
				system("open https://github.com/dfranx/SHADERed/blob/master/TUTORIAL.md"); // [MACOS]
			#elif defined(__linux__) || defined(__unix__)
				system("xdg-open https://github.com/dfranx/SHADERed/blob/master/TUTORIAL.md");
			#elif defined(_WIN32)
				ShellExecuteW(NULL, L"open", L"https://github.com/dfranx/SHADERed/blob/master/TUTORIAL.md", NULL, NULL, SW_SHOWNORMAL);
			#endif
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.Options", [=]() {
			m_optionsOpened = true;
			*m_settingsBkp = Settings::Instance();
			m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.ChangeThemeUp", [=]() {
			std::vector<std::string> themes = ((OptionsUI*)m_options)->GetThemeList();

			std::string& theme = Settings::Instance().Theme;

			for (int i = 0; i < themes.size(); i++) {
				if (theme == themes[i]) {
					if (i != 0)
						theme = themes[i - 1];
					else
						theme = themes[themes.size() - 1];
					break;
				}
			}

			((OptionsUI*)m_options)->ApplyTheme();
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.ChangeThemeDown", [=]() {
			std::vector<std::string> themes = ((OptionsUI*)m_options)->GetThemeList();

			std::string& theme = Settings::Instance().Theme;

			for (int i = 0; i < themes.size(); i++) {
				if (theme == themes[i]) {
					if (i != themes.size() - 1)
						theme = themes[i + 1];
					else
						theme = themes[0];
					break;
				}
			}

			((OptionsUI*)m_options)->ApplyTheme();
		});

		KeyboardShortcuts::Instance().SetCallback("Window.Exit", [=]() {
			SDL_DestroyWindow(m_wnd);
		});
	}
	void GUIManager::m_loadTemplateList()
	{
		m_selectedTemplate = "";

		Logger::Get().Log("Loading template list");
		
		for (const auto & entry : ghc::filesystem::directory_iterator("templates")) {
			std::string file = entry.path().filename().native();
			m_templates.push_back(file);
			
			if (file == Settings::Instance().General.StartUpTemplate)
				m_selectedTemplate = file;
		}

		if (m_selectedTemplate.size() == 0) {
			if (m_templates.size() != 0) {
				Logger::Get().Log("Default template set to " + m_selectedTemplate);
				m_selectedTemplate = m_templates[0];
			} else {
				m_selectedTemplate = "?empty";
				Logger::Get().Log("No templates found", true);
			}
		}

		m_data->Parser.SetTemplate(m_selectedTemplate);
	}
}