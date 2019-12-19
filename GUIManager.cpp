#include "GUIManager.h"
#include "InterfaceManager.h"
#include <SDL2/SDL_messagebox.h>
#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_opengl3.h>
#include <imgui/examples/imgui_impl_sdl.h>
#include "UI/ObjectPreviewUI.h"
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
#include "UI/Icons.h"
#include "Objects/Logger.h"
#include "Objects/Names.h"
#include "Objects/Settings.h"
#include "Objects/ThemeContainer.h"
#include "Objects/CameraSnapshots.h"
#include "Objects/KeyboardShortcuts.h"
#include "Objects/FunctionVariableManager.h"
#include "Objects/SystemVariableManager.h"

#include <fstream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#define STBIR_DEFAULT_FILTER_DOWNSAMPLE   STBIR_FILTER_CATMULLROM
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize.h>
/*	STBIR_FILTER_CUBICBSPLINE
	STBIR_FILTER_CATMULLROM
	STBIR_FILTER_MITCHELL		*/

#include <ghc/filesystem.hpp>

#if defined(__APPLE__)
	// no includes on mac os
#elif defined(__linux__) || defined(__unix__)
	// no includes on linux
#elif defined(_WIN32)
	#include <windows.h>
#endif

#define HARRAYSIZE(a) (sizeof(a)/sizeof(*a))
#define TOOLBAR_HEIGHT 48

#define getByte(value, n) (value >> (n*8) & 0xFF)

namespace ed
{
	float clip(float n, float lower, float upper) {
		return std::max(lower, std::min(n, upper));
	}

	GUIManager::GUIManager(ed::InterfaceManager* objects, SDL_Window* wnd, SDL_GLContext* gl)
	{
		m_data = objects;
		m_wnd = wnd;
		m_gl  = gl;
		m_settingsBkp = new Settings();
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
		m_isCreateBufferOpened = false;
		m_isNewProjectPopupOpened = false;
		m_isUpdateNotificationOpened = false;
		m_isRecordCameraSnapshotOpened = false;
		m_isCreateImgOpened = false;
		m_isAboutOpen = false;
		m_wasPausedPrior = true;
		m_savePreviewSeq = false;
		m_cacheProjectModified = false;
		m_isCreateImg3DOpened = false;
		m_isInfoOpened = false;
		m_savePreviewSeqDuration = 5.5f;
		m_savePreviewSeqFPS = 30;
		m_savePreviewSupersample = 0;
		m_iconFontLarge = nullptr;

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

		m_views.push_back(new PreviewUI(this, objects, "Preview"));
		m_views.push_back(new PinnedUI(this, objects, "Pinned"));
		m_views.push_back(new CodeEditorUI(this, objects, "Code"));
		m_views.push_back(new MessageOutputUI(this, objects, "Output"));
		m_views.push_back(new ObjectListUI(this, objects, "Objects"));
		m_views.push_back(new PipelineUI(this, objects, "Pipeline"));
		m_views.push_back(new PropertyUI(this, objects, "Properties"));

		KeyboardShortcuts::Instance().Load();
		m_setupShortcuts();

		m_options = new OptionsUI(this, objects, "Options");
		m_createUI = new CreateItemUI(this, objects);
		m_objectPrev = new ObjectPreviewUI(this, objects, "Object Preview");

		// turn on the tracker on startup
		((CodeEditorUI*)Get(ViewID::Code))->SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange);
		((CodeEditorUI*)Get(ViewID::Code))->SetAutoRecompile(Settings::Instance().General.AutoRecompile);

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

		if (Settings::Instance().General.CheckUpdates) {
			m_updateCheck.CheckForUpdates([&]() {
				m_isUpdateNotificationOpened = true;
				m_updateNotifyClock.restart();
			});
		}

	}
	GUIManager::~GUIManager()
	{
		// turn off the tracker on exit
		((CodeEditorUI*)Get(ViewID::Code))->SetTrackFileChanges(false);

		Logger::Get().Log("Shutting down UI");

		for (auto& view : m_views)
			delete view;

		delete m_options;
		delete m_objectPrev;
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

				if (!(ImGui::GetIO().WantTextInput && !codeHasFocus))
					KeyboardShortcuts::Instance().Check(e, codeHasFocus);
			}
		}
		else if (e.type == SDL_MOUSEMOTION)
			m_perfModeClock.restart();
		else if (e.type == SDL_DROPFILE) {
			
			char* droppedFile = e.drop.file;
			
			std::string file = m_data->Parser.GetRelativePath(droppedFile);
			size_t dotPos = file.find_last_of('.');

			if (!file.empty() && dotPos != std::string::npos) {
				std::string ext = file.substr(dotPos + 1);

				const std::vector<std::string> imgExt = { "png", "jpeg", "jpg", "bmp", "gif", "psd", "pic", "pnm", "hdr", "tga" };
				const std::vector<std::string> sndExt = { "ogg", "wav", "flac", "aiff", "raw" }; // TODO: more file ext
				const std::vector<std::string> projExt = { "sprj" };

				if (std::count(projExt.begin(), projExt.end(), ext) > 0) {
					bool cont = true;
					if (m_data->Parser.IsProjectModified()) {
						int btnID = this->AreYouSure();
						if (btnID == 2)
							cont = false;
					}

					if (cont)
						Open(m_data->Parser.GetProjectPath(file));
				}
				else if (std::count(imgExt.begin(), imgExt.end(), ext) > 0)
					m_data->Objects.CreateTexture(file);
				else if (std::count(sndExt.begin(), sndExt.end(), ext) > 0)
					m_data->Objects.CreateAudio(file);
				else m_data->Plugins.HandleDropFile(file.c_str());
			}

			SDL_free(droppedFile);
		}
		else if (e.type == SDL_WINDOWEVENT) {
			if (e.window.event == SDL_WINDOWEVENT_MOVED ||
				e.window.event == SDL_WINDOWEVENT_MAXIMIZED ||
				e.window.event == SDL_WINDOWEVENT_RESIZED)
			{
				SDL_GetWindowSize(m_wnd, &m_width, &m_height);
			}
		}

		if (m_optionsOpened)
			m_options->OnEvent(e);

		if (((ed::ObjectPreviewUI*)m_objectPrev)->ShouldRun())
			m_objectPrev->OnEvent(e);

		for (auto& view : m_views)
			view->OnEvent(e);

		m_data->Plugins.OnEvent(e);
	}
	void GUIManager::m_tooltip(const std::string &text)
	{
		if (ImGui::IsItemHovered())
		{
			ImGui::PopFont(); // TODO: remove this if its being used in non-toolbar places

			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(text.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();

			ImGui::PushFont(m_iconFontLarge);
		}
	}
	void GUIManager::m_renderToolbar()
	{
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + ImGui::GetFrameHeight()));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, TOOLBAR_HEIGHT * Settings::Instance().DPIScale));
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("##toolbar", 0, window_flags);
		ImGui::PopStyleVar(3);
		
		float bHeight = TOOLBAR_HEIGHT/2 + ImGui::GetStyle().FramePadding.y * 2;
		ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2 - bHeight * Settings::Instance().DPIScale / 2);
		ImGui::Indent(15 * Settings::Instance().DPIScale);

		/*
			project (open, open directory, save, empty, new shader) ||
			objects (new texture, new cubemap, new audio, new render texture) ||
			preview (screenshot, pause, zoom in, zoom out, maximize) ||
			random (settings)
		*/
		ImGui::Columns(4);
		
		ImGui::SetColumnWidth(0, (5*(TOOLBAR_HEIGHT) + 5*2*ImGui::GetStyle().FramePadding.x) * Settings::Instance().DPIScale);
		ImGui::SetColumnWidth(1, (8*(TOOLBAR_HEIGHT) + 8*2*ImGui::GetStyle().FramePadding.x) * Settings::Instance().DPIScale);
		ImGui::SetColumnWidth(2, (4*(TOOLBAR_HEIGHT) + 4*2*ImGui::GetStyle().FramePadding.x) * Settings::Instance().DPIScale);
		
		ImGui::PushFont(m_iconFontLarge);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		// TODO: maybe pack all these into functions such as m_open, m_createEmpty, etc... so that there are no code
		// repetitions
		if (ImGui::Button(UI_ICON_DOCUMENT_FOLDER)) {		// OPEN PROJECT
			bool cont = true;
			if (m_data->Parser.IsProjectModified()) {
				int btnID = this->AreYouSure();
				if (btnID == 2)
					cont = false;
			}

			if (cont) {
				std::string file;
				if (UIHelper::GetOpenFileDialog(file, "sprj"))
					Open(file);
			}
		}
		m_tooltip("Open a project");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_SAVE))					// SAVE
			Save();
		m_tooltip("Save project");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_FILE)) { // EMPTY PROJECT
			m_selectedTemplate = "?empty";
			m_isNewProjectPopupOpened = true;
		}
		m_tooltip("New empty project");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FOLDER_OPEN)) {			// OPEN DIRECTORY
			std::string prpath = m_data->Parser.GetProjectPath("");
#if defined(__APPLE__)
			system(("open " + prpath).c_str()); // [MACOS]
#elif defined(__linux__) || defined(__unix__)
			system(("xdg-open " + prpath).c_str());
#elif defined(_WIN32)
			ShellExecuteA(NULL, "open", prpath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
		}
		m_tooltip("Open project directory");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_CODE)) {				// NEW SHADER FILE
			std::string file;
			bool success = UIHelper::GetSaveFileDialog(file, "hlsl;glsl;vert;frag;geom");

			if (success) {
				// create a file (cross platform)
				std::ofstream ofs(file);
				ofs << "// empty shader file\n";
				ofs.close();
			}
		}
		m_tooltip("New shader file");
		ImGui::NextColumn();

		if (ImGui::Button(UI_ICON_PIXELS)) this->CreateNewShaderPass();
		m_tooltip("New shader pass");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FX)) this->CreateNewComputePass();
		m_tooltip("New compute pass");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_IMAGE)) this->CreateNewTexture();
		m_tooltip("New texture");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_IMAGE)) this->CreateNewImage();
		m_tooltip("New empty image");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_WAVE)) this->CreateNewAudio();
		m_tooltip("New audio");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_TRANSPARENT)) this->CreateNewRenderTexture();
		m_tooltip("New render texture");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_CUBE)) this->CreateNewCubemap();
		m_tooltip("New cubemap");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_TEXT)) this->CreateNewBuffer();
		m_tooltip("New buffer");
		ImGui::NextColumn();

		if (ImGui::Button(UI_ICON_REFRESH)) {				// REBUILD PROJECT
			((CodeEditorUI*)Get(ViewID::Code))->SaveAll();
			std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
			for (PipelineItem*& pass : passes)
				m_data->Renderer.Recompile(pass->Name);
		}
		m_tooltip("Rebuild");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_CAMERA)) m_savePreviewPopupOpened = true; // TAKE A SCREENSHOT
		m_tooltip("Render");
		ImGui::SameLine();
		if (ImGui::Button(m_data->Renderer.IsPaused() ? UI_ICON_PLAY : UI_ICON_PAUSE))
			m_data->Renderer.Pause(!m_data->Renderer.IsPaused());
		m_tooltip("Pause preview");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_MAXIMIZE)) m_perfModeFake = !m_perfModeFake;
		m_tooltip("Performance mode");
		ImGui::NextColumn();

		if (ImGui::Button(UI_ICON_GEAR)) {				// SETTINGS BUTTON 
			m_optionsOpened = true;
			*m_settingsBkp = Settings::Instance();
			m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
		}
		m_tooltip("Settings");
		ImGui::NextColumn();	

		ImGui::PopStyleColor();
		ImGui::PopFont();
		ImGui::Columns(1);


		ImGui::End();
	}
	int GUIManager::AreYouSure()
	{
		const SDL_MessageBoxButtonData buttons[] = {
			{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "CANCEL" },
			{ /* .flags, .buttonid, .text */        0, 1, "NO" },
			{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "YES" },
		};
		const SDL_MessageBoxData messageboxdata = {
			SDL_MESSAGEBOX_INFORMATION, /* .flags */
			m_wnd, /* .window */
			"SHADERed", /* .title */
			"Save changes to the project before quitting?", /* .message */
			SDL_arraysize(buttons), /* .numbuttons */
			buttons, /* .buttons */
			NULL /* .colorScheme */
		};
		int buttonid;
		if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0) {
			Logger::Get().Log("Failed to open message box.", true);
			return -1;
		}

		if (buttonid == 0) // save
			Save();

		return buttonid;
	}
	void GUIManager::Update(float delta)
	{
		// add star to the titlebar if project was modified
		if (m_cacheProjectModified != m_data->Parser.IsProjectModified()) {
			std::string projName = m_data->Parser.GetOpenedFile();

			if (projName.size() > 0) {
				projName = projName.substr(projName.find_last_of("/\\") + 1);
				
				if (m_data->Parser.IsProjectModified())
					projName = "*" + projName;

				SDL_SetWindowTitle(m_wnd, ("SHADERed (" + projName + ")").c_str());
			} else {
				if (m_data->Parser.IsProjectModified())
					SDL_SetWindowTitle(m_wnd, "SHADERed (*)");
				else
					SDL_SetWindowTitle(m_wnd, "SHADERed");
			}

			m_cacheProjectModified = m_data->Parser.IsProjectModified();
		}

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

			ImFont* font = fonts->AddFontFromFileTTF(m_cachedFont.c_str(), m_cachedFontSize * Settings::Instance().DPIScale);

			// icon font
  			ImGuiIO& io = ImGui::GetIO();
			ImFontConfig config;
			config.MergeMode = true;
  			static const ImWchar icon_ranges[] = { 0xea5b, 0xf026, 0 };
			io.Fonts->AddFontFromFileTTF("data/icofont.ttf", m_cachedFontSize * Settings::Instance().DPIScale, &config, icon_ranges);
			
			ImFont* edFontPtr = fonts->AddFontFromFileTTF(edFont.first.c_str(), edFont.second * Settings::Instance().DPIScale);

			if (font == nullptr || edFontPtr == nullptr) {
				fonts->Clear();
				font = fonts->AddFontDefault();
				edFontPtr = fonts->AddFontDefault();

				Logger::Get().Log("Failed to load fonts", true);
			}

			// icon font large
			ImFontConfig configIconsLarge;
			m_iconFontLarge = io.Fonts->AddFontFromFileTTF("data/icofont.ttf", (TOOLBAR_HEIGHT/2) * Settings::Instance().DPIScale, &configIconsLarge, icon_ranges);

			ImGui::GetIO().FontDefault = font;
			fonts->Build();

			ImGui_ImplOpenGL3_DestroyFontsTexture();

			((CodeEditorUI*)Get(ViewID::Code))->UpdateFont();
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_wnd);
		ImGui::NewFrame();

		// toolbar
		static bool initializedToolbar = false;
		bool actuallyToolbar = settings.General.Toolbar && !m_performanceMode;
		if (!initializedToolbar) { // some hacks ew
			m_renderToolbar();
			initializedToolbar = true;
		} else if (actuallyToolbar)
			m_renderToolbar();

		// create a fullscreen imgui panel that will host a dockspace
		bool showMenu = !(m_performanceMode && settings.Preview.HideMenuInPerformanceMode && m_perfModeClock.getElapsedTime().asSeconds() > 2.5f);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | (ImGuiWindowFlags_MenuBar * showMenu) | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + actuallyToolbar * TOOLBAR_HEIGHT * Settings::Instance().DPIScale));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - actuallyToolbar * TOOLBAR_HEIGHT * Settings::Instance().DPIScale));
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
			std::vector<bool> needsUpdate = ((CodeEditorUI*)Get(ViewID::Code))->TrackedNeedsUpdate();
			std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
			int ind = 0;
			for (PipelineItem*& pass : passes) {
				if (needsUpdate[ind])
					m_data->Renderer.Recompile(pass->Name);
				ind++;
			}
			((CodeEditorUI*)Get(ViewID::Code))->EmptyTrackedFiles();
		}
		((CodeEditorUI*)Get(ViewID::Code))->UpdateAutoRecompileItems();

		// menu
		if (showMenu && ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::BeginMenu("New")) {
					if (ImGui::MenuItem("Empty")) {
						bool cont = true;
						if (m_data->Parser.IsProjectModified()) {
							int btnID = this->AreYouSure();
							if (btnID == 2)
								cont = false;
						}

						if (cont) {
							m_selectedTemplate = "?empty";
							m_isNewProjectPopupOpened = true;
						}
					}
					ImGui::Separator();

					for (int i = 0; i < m_templates.size(); i++)
						if (ImGui::MenuItem(m_templates[i].c_str())) {
							bool cont = true;
							if (m_data->Parser.IsProjectModified()) {
								int btnID = this->AreYouSure();
								if (btnID == 2)
									cont = false;
							}

							if (cont) {
								m_selectedTemplate = m_templates[i];
								m_isNewProjectPopupOpened = true;
							}
						}

					m_data->Plugins.ShowMenuItems("newproject");

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
					bool cont = true;
					if (m_data->Parser.IsProjectModified()) {
						int btnID = this->AreYouSure();
						if (btnID == 2)
							cont = false;
					}

					if (cont) {
						std::string file;
						bool success = UIHelper::GetOpenFileDialog(file, "sprj");

						if (success)
							Open(file);
					}
				}
				if (ImGui::MenuItem("Save", KeyboardShortcuts::Instance().GetString("Project.Save").c_str()))
					this->Save();
				if (ImGui::MenuItem("Save As", KeyboardShortcuts::Instance().GetString("Project.SaveAs").c_str()))
					SaveAsProject(true);
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
				
				m_data->Plugins.ShowMenuItems("file");

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
					if (ImGui::MenuItem("Shader Pass", KeyboardShortcuts::Instance().GetString("Project.NewShaderPass").c_str()))
						this->CreateNewShaderPass();
					if (ImGui::MenuItem("Compute Pass", KeyboardShortcuts::Instance().GetString("Project.NewComputePass").c_str()))
						this->CreateNewComputePass();
					if (ImGui::MenuItem("Audio Pass", KeyboardShortcuts::Instance().GetString("Project.NewAudioPass").c_str()))
						this->CreateNewAudioPass();
					ImGui::Separator();
					if (ImGui::MenuItem("Texture", KeyboardShortcuts::Instance().GetString("Project.NewTexture").c_str()))
						this->CreateNewTexture();
					if (ImGui::MenuItem("Cubemap", KeyboardShortcuts::Instance().GetString("Project.NewCubeMap").c_str()))
						this->CreateNewCubemap();
					if (ImGui::MenuItem("Audio", KeyboardShortcuts::Instance().GetString("Project.NewAudio").c_str()))
						this->CreateNewAudio();
					if (ImGui::MenuItem("Render Texture", KeyboardShortcuts::Instance().GetString("Project.NewRenderTexture").c_str()))
						this->CreateNewRenderTexture();
					if (ImGui::MenuItem("Buffer", KeyboardShortcuts::Instance().GetString("Project.NewBuffer").c_str()))
						this->CreateNewBuffer();
					if (ImGui::MenuItem("Empty image", KeyboardShortcuts::Instance().GetString("Project.NewImage").c_str()))
						this->CreateNewImage();
					if (ImGui::MenuItem("Empty 3D image", KeyboardShortcuts::Instance().GetString("Project.NewImage3D").c_str()))
						this->CreateNewImage3D();

					m_data->Plugins.ShowMenuItems("createitem");

					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Camera snapshots")) {
					if (ImGui::MenuItem("Add", KeyboardShortcuts::Instance().GetString("Project.CameraSnapshot").c_str())) CreateNewCameraSnapshot();
					if (ImGui::BeginMenu("Delete")) {
						auto& names = CameraSnapshots::GetList();
						for (const auto& name : names)
							if (ImGui::MenuItem(name.c_str()))
								CameraSnapshots::Remove(name);
						ImGui::EndMenu();
					}
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Reset time"))
					SystemVariableManager::Instance().Reset();
				if (ImGui::MenuItem("Options")) {
					m_optionsOpened = true;
					((OptionsUI*)m_options)->SetGroup(ed::OptionsUI::Page::Project);
					m_optGroup = (int)OptionsUI::Page::Project;
					*m_settingsBkp = settings;
					m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
				}

				m_data->Plugins.ShowMenuItems("project");
				
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Window")) {
				for (auto& view : m_views) {
					if (view->Name != "Code") // dont show the "Code" UI view in this menu
						ImGui::MenuItem(view->Name.c_str(), 0, &view->Visible);
				}

				m_data->Plugins.ShowMenuItems("window");

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
				if (ImGui::BeginMenu("Support")) {
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
				if (ImGui::MenuItem("Information")) { m_isInfoOpened = true; }
				if (ImGui::MenuItem("About SHADERed")) { m_isAboutOpen = true; }

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Supporters")) {
				static const std::vector<std::pair<std::string, std::string>> slist = {
					std::make_pair("Hugo Locurcio", "https://github.com/Calinou")
				};

				for (auto& sitem : slist) {
					if (ImGui::MenuItem(sitem.first.c_str())) {
#if defined(__APPLE__)
						system(("open " + sitem.second).c_str()); // [MACOS]
#elif defined(__linux__) || defined(__unix__)
						system(("xdg-open " + sitem.second).c_str());
#elif defined(_WIN32)
						std::wstring wstr(sitem.second.begin(), sitem.second.end());
						ShellExecuteW(NULL, L"open", wstr.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
					}
				}

				ImGui::EndMenu();
			}
			m_data->Plugins.ShowCustomMenu();
			ImGui::EndMainMenuBar();
		}

		if (m_performanceMode)
			((PreviewUI*)Get(ViewID::Preview))->Update(delta);

		ImGui::End();


		if (!m_performanceMode) {
			for (auto& view : m_views)
				if (view->Visible) {
					ImGui::SetNextWindowSizeConstraints(ImVec2(80, 80), ImVec2(m_width*2, m_height*2));
					if (ImGui::Begin(view->Name.c_str(), &view->Visible)) view->Update(delta);
					ImGui::End();
				}

			m_data->Plugins.Update(delta);
			Get(ViewID::Code)->Update(delta);
		}

		// object preview
		if (((ed::ObjectPreviewUI*)m_objectPrev)->ShouldRun() && !m_performanceMode)
			m_objectPrev->Update(delta);

		// handle the "build occured" event
		if (settings.General.AutoOpenErrorWindow && m_data->Messages.BuildOccured) {
			size_t errors = m_data->Messages.GetErrorAndWarningMsgCount();
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
			m_wasPausedPrior = m_data->Renderer.IsPaused();
			m_savePreviewCachedTime = m_savePreviewTime = SystemVariableManager::Instance().GetTime();
			m_savePreviewTimeDelta = SystemVariableManager::Instance().GetTimeDelta();
			m_savePreviewCachedFIndex = m_savePreviewFrameIndex = SystemVariableManager::Instance().GetFrameIndex();
			glm::ivec4 wasd = SystemVariableManager::Instance().GetKeysWASD();
			m_savePreviewWASD[0] = wasd.x; m_savePreviewWASD[1] = wasd.y;
			m_savePreviewWASD[2] = wasd.z; m_savePreviewWASD[3] = wasd.w;
			m_savePreviewMouse = SystemVariableManager::Instance().GetMouse();
			m_savePreviewSupersample = 0;
			
			m_savePreviewSeq = false;
			
			m_data->Renderer.Pause(true);
		}

		// open popup for creating cubemap
		if (m_isCreateCubemapOpened) {
			ImGui::OpenPopup("Create cubemap##main_create_cubemap");
			m_isCreateCubemapOpened = false;
		}

		// open popup for creating buffer
		if (m_isCreateBufferOpened) {
			ImGui::OpenPopup("Create buffer##main_create_buffer");
			m_isCreateBufferOpened = false;
		}

		// open popup for creating image
		if (m_isCreateImgOpened) {
			ImGui::OpenPopup("Create image##main_create_img");
			m_isCreateImgOpened = false;
		}

		// open popup for creating image3D
		if (m_isCreateImg3DOpened) {
			ImGui::OpenPopup("Create 3D image##main_create_img3D");
			m_isCreateImg3DOpened = false;
		}

		// open popup for creating camera snapshot
		if (m_isRecordCameraSnapshotOpened) {
			ImGui::OpenPopup("Camera snapshot name##main_camsnap_name");
			m_isRecordCameraSnapshotOpened = false;
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

		// open tips window
		if (m_isInfoOpened) {
			ImGui::OpenPopup("Information##main_info");
			m_isInfoOpened = false;
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
				if (m_data->Objects.CreateRenderTexture(buf)) {
					((PropertyUI*)Get(ViewID::Properties))->Open(buf, m_data->Objects.GetObjectManagerItem(buf));
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Record cam snapshot
		ImGui::SetNextWindowSize(ImVec2(430 * Settings::Instance().DPIScale, 175 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Camera snapshot name##main_camsnap_name")) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 64);

			if (ImGui::Button("Ok")) {
				bool exists = false;
				auto& names = CameraSnapshots::GetList();
				for (const auto& name : names) {
					if (strcmp(buf, name.c_str()) == 0) {
						exists = true;
						break;
					}
				}

				if (!exists) {
					CameraSnapshots::Add(buf, SystemVariableManager::Instance().GetViewMatrix());
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create buffer popup
		ImGui::SetNextWindowSize(ImVec2(430 * Settings::Instance().DPIScale, 175 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Create buffer##main_create_buffer")) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 64);

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateBuffer(buf))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create empty image popup
		ImGui::SetNextWindowSize(ImVec2(430 * Settings::Instance().DPIScale, 175 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Create image##main_create_img"))
		{
			static char buf[65] = {0};
			static glm::ivec2 size(0,0);

			ImGui::InputText("Name", buf, 64);
			ImGui::DragInt2("Size", glm::value_ptr(size));
			if (size.x <= 0) size.x = 1;
			if (size.y <= 0) size.y = 1;

			if (ImGui::Button("Ok"))
			{
				if (m_data->Objects.CreateImage(buf, size))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create empty 3D image
		ImGui::SetNextWindowSize(ImVec2(430 * Settings::Instance().DPIScale, 175 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Create 3D image##main_create_img3D"))
		{
			static char buf[65] = { 0 };
			static glm::ivec3 size(0, 0, 0);

			ImGui::InputText("Name", buf, 64);
			ImGui::DragInt3("Size", glm::value_ptr(size));
			if (size.x <= 0) size.x = 1;
			if (size.y <= 0) size.y = 1;
			if (size.z <= 0) size.z = 1;

			if (ImGui::Button("Ok"))
			{
				if (m_data->Objects.CreateImage3D(buf, size))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}


		// Create about popup
		ImGui::SetNextWindowSize(ImVec2(270 * Settings::Instance().DPIScale, 180 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("About##main_about")) {
			ImGui::TextWrapped("(C) 2019 dfranx");
			ImGui::TextWrapped("Version 1.3");
			ImGui::TextWrapped("Internal version: %d", UpdateChecker::MyVersion);
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

			ImGui::Separator();

			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create "Tips" popup
		ImGui::SetNextWindowSize(ImVec2(550 * Settings::Instance().DPIScale, 560 * Settings::Instance().DPIScale), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Information##main_info")) {
			ImGui::TextWrapped("Here you can find random information about various features");

			ImGui::TextWrapped("System variables");
			ImGui::Separator();
			ImGui::TextWrapped("Time (float) - time elapsed since app start");
			ImGui::TextWrapped("TimeDelta (float) - render time");
			ImGui::TextWrapped("FrameIndex (uint) - current frame index");
			ImGui::TextWrapped("ViewportSize (vec2) - rendering window size");
			ImGui::TextWrapped("MousePosition (vec2) - normalized mouse position in the Preview window");
			ImGui::TextWrapped("View (mat4) - a built-in camera matrix");
			ImGui::TextWrapped("Projection (mat4) - a built-in projection matrix");
			ImGui::TextWrapped("ViewProjection (mat4) - View*Projection matrix");
			ImGui::TextWrapped("Orthographic (mat4) - an orthographic matrix");
			ImGui::TextWrapped("ViewOrthographic (mat4) - View*Orthographic");
			ImGui::TextWrapped("GeometryTransform (mat4) - applies Scale, Rotation and Position to geometry");
			ImGui::TextWrapped("IsPicked (bool) - check if current item is selected");
			ImGui::TextWrapped("CameraPosition (vec4) - current camera position");
			ImGui::TextWrapped("CameraPosition3 (vec3) - current camera position");
			ImGui::TextWrapped("CameraDirection3 (vec3) - camera view direction");
			ImGui::TextWrapped("KeysWASD (vec4) - W, A, S or D keys state");
			ImGui::TextWrapped("Mouse (vec4) - vec4(x,y,left,right) updated every frame");
			ImGui::TextWrapped("MouseButton (vec4) - vec4(x,y,clickX,clickY) updated only when left mouse button is down");

			ImGui::NewLine();
			ImGui::Separator();
			ImGui::TextWrapped("Have an idea for a feature that's missing? Suggest it ");
			ImGui::SameLine();
			if (ImGui::Button("here")) {
#if defined(__APPLE__)
				system("open https://github.com/dfranx/SHADERed/issues"); // [MACOS]
#elif defined(__linux__) || defined(__unix__)
				system("xdg-open https://github.com/dfranx/SHADERed/issues");
#elif defined(_WIN32)
				ShellExecuteW(NULL, L"open", L"https://github.com/dfranx/SHADERed/issues", NULL, NULL, SW_SHOWNORMAL);
#endif
			}

			ImGui::Separator();

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
						((ObjectPreviewUI*)Get(ViewID::ObjectPreview))->CloseAll();
						CameraSnapshots::Clear();
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
					((ObjectPreviewUI*)Get(ViewID::ObjectPreview))->CloseAll();
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
						((ObjectPreviewUI*)Get(ViewID::ObjectPreview))->CloseAll();
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
		ImGui::SetNextWindowSize(ImVec2(450, 250), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Save Preview##main_save_preview")) {
			ImGui::TextWrapped("Path: %s", m_previewSavePath.c_str());
			ImGui::SameLine();
			if (ImGui::Button("...##save_prev_path"))
				UIHelper::GetSaveFileDialog(m_previewSavePath, "png;jpg,jpeg;bmp;tga");
			
			ImGui::Text("Width: ");
			ImGui::SameLine();
			ImGui::Indent(105);
			ImGui::InputInt("##save_prev_sizew", &m_previewSaveSize.x);
			ImGui::Unindent(105);

			ImGui::Text("Height: ");
			ImGui::SameLine();
			ImGui::Indent(105);
			ImGui::InputInt("##save_prev_sizeh", &m_previewSaveSize.y);
			ImGui::Unindent(105);

			ImGui::Text("Supersampling: ");
			ImGui::SameLine();
			ImGui::Indent(105);
			ImGui::Combo("##save_prev_ssmp", &m_savePreviewSupersample, " 1x\0 2x\0 4x\0 8x\0");
			ImGui::Unindent(105);

			ImGui::Separator();
			if (ImGui::CollapsingHeader("Sequence")) {
				ImGui::TextWrapped("Export a sequence of images");

				/* RECORD */
				ImGui::Text("Record:");
				ImGui::SameLine();
				ImGui::Checkbox("##save_prev_keyw", &m_savePreviewSeq);

				if (!m_savePreviewSeq) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}

				/* DURATION */
				ImGui::Text("Duration:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat("##save_prev_seqdur", &m_savePreviewSeqDuration);
				ImGui::PopItemWidth();

				/* TIME DELTA */
				ImGui::Text("FPS:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::DragInt("##save_prev_seqfps", &m_savePreviewSeqFPS);
				ImGui::PopItemWidth();

				if (!m_savePreviewSeq) {
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}
			}
			ImGui::Separator();

			ImGui::Separator();
			if (ImGui::CollapsingHeader("Advanced")) {
				/* TIME */
				ImGui::Text("Time:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##save_prev_time", &m_savePreviewTime)) {
					float timeAdvance = m_savePreviewTime - SystemVariableManager::Instance().GetTime();
					SystemVariableManager::Instance().AdvanceTimer(timeAdvance);
				}
				ImGui::PopItemWidth();

				/* TIME DELTA */
				ImGui::Text("Time delta:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat("##save_prev_timed", &m_savePreviewTimeDelta);
				ImGui::PopItemWidth();

				/* FRAME INDEX */
				ImGui::Text("Frame index:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::DragInt("##save_prev_findex", &m_savePreviewFrameIndex);
				ImGui::PopItemWidth();

				/* WASD */
				ImGui::Text("WASD:"); ImGui::SameLine();
				ImGui::Checkbox("##save_prev_keyw", &m_savePreviewWASD[0]); ImGui::SameLine();
				ImGui::Checkbox("##save_prev_keya", &m_savePreviewWASD[1]); ImGui::SameLine();
				ImGui::Checkbox("##save_prev_keys", &m_savePreviewWASD[2]); ImGui::SameLine();
				ImGui::Checkbox("##save_prev_keyd", &m_savePreviewWASD[3]);

				/* MOUSE */
				ImGui::Text("Mouse:");
				ImGui::SameLine();
				if (ImGui::DragFloat2("##save_prev_mpos", glm::value_ptr(m_savePreviewMouse)))
					SystemVariableManager::Instance().SetMousePosition(m_savePreviewMouse.x, m_savePreviewMouse.y);
				ImGui::SameLine();
				bool isLeftDown = m_savePreviewMouse.z >= 1.0f;
				ImGui::Checkbox("##save_prev_btnleft", &isLeftDown); ImGui::SameLine();
				m_savePreviewMouse.z = isLeftDown;
				bool isRightDown = m_savePreviewMouse.w >= 1.0f;
				ImGui::Checkbox("##save_prev_btnright", &isRightDown);
				m_savePreviewMouse.w = isRightDown;
				SystemVariableManager::Instance().SetMouse(m_savePreviewMouse.x, m_savePreviewMouse.y, m_savePreviewMouse.z, m_savePreviewMouse.w);
			}
			ImGui::Separator();

			if (ImGui::Button("Save")) {
				int sizeMulti = 1;
				switch (m_savePreviewSupersample) {
				case 1: sizeMulti = 2; break;
				case 2: sizeMulti = 4; break;
				case 3: sizeMulti = 8; break;
				}
				int actualSizeX = m_previewSaveSize.x * sizeMulti;
				int actualSizeY = m_previewSaveSize.y * sizeMulti;

				// normal render
				if (!m_savePreviewSeq) {
					if (actualSizeX > 0 && actualSizeY > 0) {
						SystemVariableManager::Instance().CopyState();
						
						SystemVariableManager::Instance().SetTimeDelta(m_savePreviewTimeDelta);
						SystemVariableManager::Instance().SetFrameIndex(m_savePreviewFrameIndex);
						SystemVariableManager::Instance().SetKeysWASD(m_savePreviewWASD[0], m_savePreviewWASD[1], m_savePreviewWASD[2], m_savePreviewWASD[3]);
						SystemVariableManager::Instance().SetMousePosition(m_savePreviewMouse.x, m_savePreviewMouse.y);
						SystemVariableManager::Instance().SetMouse(m_savePreviewMouse.x, m_savePreviewMouse.y, m_savePreviewMouse.z, m_savePreviewMouse.w);
						
						m_data->Renderer.Render(actualSizeX, actualSizeY);

						SystemVariableManager::Instance().AdvanceTimer(m_savePreviewCachedTime - m_savePreviewTimeDelta);
					}

					unsigned char* pixels = (unsigned char*)malloc(actualSizeX * actualSizeY * 4);
					unsigned char* outPixels = nullptr;
					
					if (sizeMulti != 1)
						outPixels = (unsigned char*)malloc(m_previewSaveSize.x * m_previewSaveSize.y * 4);
					else outPixels = pixels;



					GLuint tex = m_data->Renderer.GetTexture();
					glBindTexture(GL_TEXTURE_2D, tex);
					glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
					glBindTexture(GL_TEXTURE_2D, 0);

					// resize image
					if (sizeMulti != 1) {
						stbir_resize_uint8(pixels, actualSizeX, actualSizeY, actualSizeX * 4,
							outPixels, m_previewSaveSize.x, m_previewSaveSize.y, m_previewSaveSize.x * 4, 4);
					}

					std::string ext = m_previewSavePath.substr(m_previewSavePath.find_last_of('.')+1);

					if (ext == "jpg" || ext == "jpeg")
						stbi_write_jpg(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels, 100);
					else if (ext == "bmp")
						stbi_write_bmp(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels);
					else if (ext == "tga")
						stbi_write_tga(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels);
					else
						stbi_write_png(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels, m_previewSaveSize.x * 4);

					if (sizeMulti != 1) free(outPixels);
					free(pixels);
				}
				else { // sequence render

					float seqDelta = 1.0f / m_savePreviewSeqFPS;

					if (actualSizeX > 0 && actualSizeY > 0) {
						SystemVariableManager::Instance().SetKeysWASD(m_savePreviewWASD[0], m_savePreviewWASD[1], m_savePreviewWASD[2], m_savePreviewWASD[3]);
						SystemVariableManager::Instance().SetMousePosition(m_savePreviewMouse.x, m_savePreviewMouse.y);
						SystemVariableManager::Instance().SetMouse(m_savePreviewMouse.x, m_savePreviewMouse.y, m_savePreviewMouse.z, m_savePreviewMouse.w);
						
						float curTime = 0.0f;
						
						GLuint tex = m_data->Renderer.GetTexture();
						
						size_t lastDot = m_previewSavePath.find_last_of('.');
						std::string ext = lastDot == std::string::npos ? "png" : m_previewSavePath.substr(lastDot+1);
						std::string filename = m_previewSavePath;
						
						// allow only one %??d
						bool inFormat = false;
						int lastFormatPos = -1;
						int formatCount = 0;
						for (int i = 0; i < filename.size(); i++) {
							if (filename[i] == '%') {
								inFormat = true;
								lastFormatPos = i;
								continue;
							}

							if (inFormat) {
								if (isdigit(filename[i])) { }
								else {
									if (filename[i] != '%' &&
										((filename[i] == 'd' && formatCount > 0) ||
											(filename[i] != 'd')))
									{
										filename.insert(lastFormatPos, 1, '%');
									}

									if (filename[i] == 'd')
										formatCount++;
									inFormat = false;
								}
							}
						}

						// no %d found? add one
						if (formatCount == 0)
							filename.insert(lastDot == std::string::npos ? filename.size() : lastDot, "%d"); // frame%d
					
						SystemVariableManager::Instance().AdvanceTimer(m_savePreviewCachedTime - m_savePreviewTimeDelta);
						SystemVariableManager::Instance().SetTimeDelta(seqDelta);

						stbi_write_png_compression_level = 5; // set to lowest compression level

						int tCount = std::thread::hardware_concurrency();
						tCount = tCount == 0 ? 2 : tCount;

						unsigned char** pixels = new unsigned char*[tCount];
						unsigned char** outPixels = new unsigned char* [tCount];
						int* curFrame = new int[tCount];
						bool* needsUpdate = new bool[tCount];
						std::thread** threadPool = new std::thread*[tCount];
						std::atomic<bool> isOver = false;

						for (int i = 0; i < tCount; i++) {
							curFrame[i] = 0;
							needsUpdate[i] = true;
							pixels[i] = (unsigned char*)malloc(actualSizeX * actualSizeY * 4);
							
							if (sizeMulti != 1) outPixels[i] = (unsigned char*)malloc(m_previewSaveSize.x * m_previewSaveSize.y * 4);
							else outPixels[i] = nullptr;

							threadPool[i] = new std::thread([ext, filename, sizeMulti, actualSizeX, actualSizeY, &outPixels, &pixels, &needsUpdate, &curFrame, &isOver](int worker, int w, int h) {
								char prevSavePath[MAX_PATH];
								while (!isOver) {
									if (needsUpdate[worker])
										continue;

									// resize image
									if (sizeMulti != 1) {
										stbir_resize_uint8(pixels[worker], actualSizeX, actualSizeY, actualSizeX * 4,
											outPixels[worker], w, h, w * 4, 4);
									} else outPixels[worker] = pixels[worker];

									sprintf(prevSavePath, filename.c_str(), curFrame[worker]);
									
									if (ext == "jpg" || ext == "jpeg")
										stbi_write_jpg(prevSavePath, w, h, 4, outPixels[worker], 100);
									else if (ext == "bmp")
										stbi_write_bmp(prevSavePath, w, h, 4, outPixels[worker]);
									else if (ext == "tga")
										stbi_write_tga(prevSavePath, w, h, 4, outPixels[worker]);
									else
										stbi_write_png(prevSavePath, w, h, 4, outPixels[worker], w * 4);
								
									needsUpdate[worker] = true;
								}
							}, i, m_previewSaveSize.x, m_previewSaveSize.y);
						}

						int globalFrame = 0;
						while (curTime < m_savePreviewSeqDuration) {
							int hasWork = -1;
							for (int i = 0; i < tCount; i++) 
								if (needsUpdate[i]) {
									hasWork = i;
									break;
								}

							if (hasWork == -1)
								continue;

							SystemVariableManager::Instance().CopyState();
							SystemVariableManager::Instance().SetFrameIndex(m_savePreviewFrameIndex + globalFrame);
							
							m_data->Renderer.Render(actualSizeX, actualSizeY);

							glBindTexture(GL_TEXTURE_2D, tex);
							glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels[hasWork]);
							glBindTexture(GL_TEXTURE_2D, 0);

							SystemVariableManager::Instance().AdvanceTimer(seqDelta);

							curTime += seqDelta;
							curFrame[hasWork] = globalFrame;
							needsUpdate[hasWork] = false;
							globalFrame++;
						}
						isOver = true;
						
						for (int i = 0; i < tCount; i++) {
							if (threadPool[i]->joinable())
								threadPool[i]->join();
							free(pixels[i]);
							if (sizeMulti != 1)
								free(outPixels[i]);
							delete threadPool[i];
						}
						delete[] pixels;
						delete[] outPixels;
						delete[] curFrame;
						delete[] needsUpdate;
						delete[] threadPool;

						stbi_write_png_compression_level = 8; // set back to default compression level
					}
				}

				m_data->Renderer.Pause(m_wasPausedPrior);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
				m_data->Renderer.Pause(m_wasPausedPrior);
			}
			ImGui::EndPopup();
		}

		// update notification
		if (m_isUpdateNotificationOpened) {
			const float DISTANCE = 15.0f;
			ImGuiIO& io = ImGui::GetIO();
			ImVec2 window_pos = ImVec2(io.DisplaySize.x - DISTANCE, io.DisplaySize.y - DISTANCE);
			ImVec2 window_pos_pivot = ImVec2(1.0f, 1.0f);
			ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
			ImGui::SetNextWindowBgAlpha(1.0f-clip(m_updateNotifyClock.getElapsedTime().asSeconds() - 5.0f, 0.0f, 2.0f) / 2.0f); // Transparent background
			ImVec4 textClr = ImGui::GetStyleColorVec4(ImGuiCol_Text);
			ImVec4 windowClr = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, textClr);
			ImGui::PushStyleColor(ImGuiCol_Text, windowClr);
			if (ImGui::Begin("##updateNotification", &m_isUpdateNotificationOpened, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
			{
				if (ImGui::IsWindowHovered())
					m_updateNotifyClock.restart();

				ImGui::Text("A new version of SHADERed is available!");
				ImGui::SameLine();
				if (ImGui::Button("UPDATE")) {
#if defined(__APPLE__)
					system("open https://github.com/dfranx/SHADERed/releases"); // [MACOS]
#elif defined(__linux__) || defined(__unix__)
					system("xdg-open https://github.com/dfranx/SHADERed/releases");
#elif defined(_WIN32)
					ShellExecuteW(NULL, L"open", L"https://github.com/dfranx/SHADERed/releases", NULL, NULL, SW_SHOWNORMAL);
#endif
					m_isUpdateNotificationOpened = false;
				}
			}
			ImGui::PopStyleColor(2);
			ImGui::End();

			if (m_updateNotifyClock.getElapsedTime().asSeconds() > 7.0f)
				m_isUpdateNotificationOpened = false;
		}

		// render ImGUI
		ImGui::Render();
	}
	void GUIManager::m_renderOptions()
	{
		OptionsUI* options = (OptionsUI*)m_options;
		static const char* optGroups[6] = {
			"General",
			"Editor",
			"Shortcuts",
			"Preview",
			"Plugins",
			"Project"
		};

		float height = abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y - ImGui::GetStyle().WindowPadding.y*2) / ImGui::GetTextLineHeightWithSpacing() - 1;
		
		ImGui::Columns(2);
		
		// TODO: this is only a temprorary fix for non-resizable columns
		static bool isColumnWidthSet = false;
		if (!isColumnWidthSet) {
			ImGui::SetColumnWidth(0, 100 * Settings::Instance().DPIScale + ImGui::GetStyle().WindowPadding.x * 2);
			isColumnWidthSet = true;
		}

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(-1);
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
			code->SetShowWhitespace(Settings::Instance().Editor.ShowWhitespace);
			code->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
			code->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
			code->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
			code->SetFont(Settings::Instance().Editor.Font, Settings::Instance().Editor.FontSize);
			code->SetHorizontalScrollbar(Settings::Instance().Editor.HorizontalScroll);
			code->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
			code->SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange);
			code->SetAutoRecompile(Settings::Instance().General.AutoRecompile);
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
		else if (view == ViewID::ObjectPreview)
			return m_objectPrev;

		return m_views[(int)view];
	}
	void GUIManager::SaveSettings()
	{
		std::ofstream data("data/gui.dat");

		for (auto& view : m_views)
			data.put(view->Visible);

		data.close();
	}
	void GUIManager::LoadSettings()
	{
		std::ifstream data("data/gui.dat");

		if (data.is_open()) {
			for (auto& view : m_views)
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
	bool GUIManager::Save()
	{
		if (m_data->Parser.GetOpenedFile() == "")
			return SaveAsProject(true);
		
		m_data->Parser.Save();

		((CodeEditorUI*)Get(ViewID::Code))->SaveAll();

		std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
		for (PipelineItem*& pass : passes)
			m_data->Renderer.Recompile(pass->Name);

		return true;
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
			((ObjectPreviewUI*)Get(ViewID::ObjectPreview))->CloseAll();

			m_data->Parser.Open(file);

			std::string projName = m_data->Parser.GetOpenedFile();
			projName = projName.substr(projName.find_last_of("/\\") + 1);

			SDL_SetWindowTitle(m_wnd, ("SHADERed (" + projName + ")").c_str());

			// returned cached state
			if (restoreCached) {
				for (auto& file : files) {
					PipelineItem* item = m_data->Pipeline.Get(file.first.c_str());

					if (file.second == 0)
						editor->OpenVS(item);
					else if (file.second == 1)
						editor->OpenPS(item);
					else if (file.second == 2)
						editor->OpenGS(item);
				}
				editor->SetOpenedFilesData(filesData);
				editor->SaveAll();
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
		((ObjectPreviewUI*)Get(ViewID::ObjectPreview))->CloseAll();
		m_data->Renderer.Pause(false); // unpause

		m_data->Parser.Open(file);

		std::string projName = m_data->Parser.GetOpenedFile();
		projName = projName.substr(projName.find_last_of("/\\") + 1);

		SDL_SetWindowTitle(m_wnd, ("SHADERed (" + projName + ")").c_str());
	}

	void GUIManager::CreateNewShaderPass() {
		m_createUI->SetType(PipelineItem::ItemType::ShaderPass);
		m_isCreateItemPopupOpened = true;
	}
	void GUIManager::CreateNewComputePass() {
		m_createUI->SetType(PipelineItem::ItemType::ComputePass);
		m_isCreateItemPopupOpened = true;
	}
	void GUIManager::CreateNewAudioPass() {
		m_createUI->SetType(PipelineItem::ItemType::AudioPass);
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
			this->Save();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.SaveAs", [=]() {
			SaveAsProject(true);
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.ToggleToolbar", [=]() {
			Settings::Instance().General.Toolbar ^= 1;
		});
		KeyboardShortcuts::Instance().SetCallback("Project.Open", [=]() {
			bool cont = true;
			if (m_data->Parser.IsProjectModified()) {
				int btnID = this->AreYouSure();
				if (btnID == 2)
					cont = false;
			}

			if (cont) {
				m_data->Renderer.FlushCache();
				std::string file;
				bool success = UIHelper::GetOpenFileDialog(file, "sprj");

				// TODO: use Open()

				if (success) {
					((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
					((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
					((PreviewUI*)Get(ViewID::Preview))->Pick(nullptr);
					((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
					((PipelineUI*)Get(ViewID::Pipeline))->Reset();
					((ObjectPreviewUI*)Get(ViewID::ObjectPreview))->CloseAll();

					m_data->Parser.Open(file);
				}
			}
		});
		KeyboardShortcuts::Instance().SetCallback("Project.New", [=]() {
			m_data->Renderer.FlushCache();
			((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
			((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
			((PreviewUI*)Get(ViewID::Preview))->Pick(nullptr);
			((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
			((PipelineUI*)Get(ViewID::Pipeline))->Reset();
			((ObjectPreviewUI*)Get(ViewID::ObjectPreview))->CloseAll();
			m_data->Pipeline.New();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewRenderTexture", [=]() {
			CreateNewRenderTexture();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewBuffer", [=]() {
			CreateNewBuffer();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewImage", [=]() {
			CreateNewImage();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewImage3D", [=]() {
			CreateNewImage3D();
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
		KeyboardShortcuts::Instance().SetCallback("Project.NewComputePass", [=]() {
			CreateNewComputePass();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewAudioPass", [=]() {
			CreateNewAudioPass();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.CameraSnapshot", [=]() {
			CreateNewCameraSnapshot();
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

		KeyboardShortcuts::Instance().SetCallback("Window.Fullscreen", [=]() {
			Uint32 wndFlags = SDL_GetWindowFlags(m_wnd);
			bool isFullscreen = wndFlags & SDL_WINDOW_FULLSCREEN_DESKTOP;

			SDL_SetWindowFullscreen(m_wnd, (!isFullscreen) * SDL_WINDOW_FULLSCREEN_DESKTOP);
		});
	}
	void GUIManager::m_loadTemplateList()
	{
		m_selectedTemplate = "";

		Logger::Get().Log("Loading template list");
		
		if (ghc::filesystem::exists("./templates/")) {
			for (const auto& entry : ghc::filesystem::directory_iterator("./templates/")) {
				std::string file = entry.path().filename().native();
				m_templates.push_back(file);

				if (file == Settings::Instance().General.StartUpTemplate)
					m_selectedTemplate = file;
			}
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