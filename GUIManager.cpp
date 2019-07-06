#include "GUIManager.h"
#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_win32.h>
#include <imgui/examples/imgui_impl_dx11.h>
#include "InterfaceManager.h"
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
#include "Objects/Names.h"
#include "Objects/Settings.h"
#include "Objects/KeyboardShortcuts.h"

#include <Windows.h>
#include <fstream>

namespace ed
{
	GUIManager::GUIManager(ed::InterfaceManager* objects, ml::Window* wnd)
	{
		m_data = objects;
		m_wnd = wnd;
		m_settingsBkp = new Settings();
		m_isCreateRTOpened = false;
		m_isCreateItemPopupOpened = false;
		m_previewSaveSize = DirectX::XMINT2(1920, 1080);
		m_savePreviewPopupOpened = false;
		m_optGroup = 0;
		m_optionsOpened = false;
		m_cachedFont = "null";
		m_cachedCustomFont = false;
		m_cachedFontSize = 15;
		m_performanceMode = false;
		m_perfModeFake = false;
		m_fontNeedsUpdate = false;

		Settings::Instance().Load();
		m_loadTemplateList();

		// set vsync on startup
		m_wnd->SetVSync(Settings::Instance().General.VSync);

		// Initialize imgui
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontDefault();
		io.IniFilename = IMGUI_INI_FILE;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
		io.ConfigDockingWithShift = false;

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowMenuButtonPosition = ImGuiDir_Right;

		ImGui_ImplWin32_Init(m_wnd->GetWindowHandle());
		ImGui_ImplDX11_Init(m_wnd->GetDevice(), m_wnd->GetDeviceContext());
		
		ImGui::StyleColorsDark();

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

		// enable dpi awareness
		ImGui_ImplWin32_EnableDpiAwareness();
		
		if (Settings::Instance().General.AutoScale)
			Settings::Instance().DPIScale = ImGui_ImplWin32_GetDpiScaleForHwnd((void*)m_wnd->GetWindowHandle());
		m_cacheUIScale = Settings::Instance().TempScale = Settings::Instance().DPIScale;

		ImGui::GetStyle().ScaleAllSizes(Settings::Instance().DPIScale);
	}
	GUIManager::~GUIManager()
	{
		// turn off the tracker on startup
		((CodeEditorUI*)Get(ViewID::Code))->SetTrackFileChanges(false);

		for (auto view : m_views)
			delete view;

		delete m_createUI;
		delete m_settingsBkp;

		// release memory
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void GUIManager::OnEvent(const ml::Event& e)
	{
		m_imguiHandleEvent(e);

		// check for shortcut presses
		if (e.Type == ml::EventType::KeyPress) {
			if (!(m_optionsOpened && ((OptionsUI*)m_options)->IsListening()))
				KeyboardShortcuts::Instance().Check(e);
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

		// update editor & workspace font
		if (((CodeEditorUI*)Get(ViewID::Code))->NeedsFontUpdate() ||
			((m_cachedCustomFont != settings.General.CustomFont ||
				m_cachedFont != settings.General.Font ||
				m_cachedFontSize != settings.General.FontSize) &&
				strcmp(settings.General.Font, "null") != 0) ||
			m_fontNeedsUpdate)
		{
			std::pair<std::string, int> edFont = ((CodeEditorUI*)Get(ViewID::Code))->GetFont();

			m_cachedCustomFont = settings.General.CustomFont;
			m_cachedFont = settings.General.Font;
			m_cachedFontSize = settings.General.FontSize;
			m_fontNeedsUpdate = false;

			ImFontAtlas* fonts = ImGui::GetIO().Fonts;
			fonts->Clear();

			ImFont* font = nullptr;
			if (!m_cachedCustomFont)
				font = fonts->AddFontDefault();
			else font = fonts->AddFontFromFileTTF(m_cachedFont.c_str(), m_cachedFontSize * Settings::Instance().DPIScale);

			ImFont* edFontPtr = fonts->AddFontFromFileTTF(edFont.first.c_str(), edFont.second * Settings::Instance().DPIScale);

			if (font == nullptr || edFontPtr == nullptr) {
				fonts->Clear();
				font = fonts->AddFontDefault();
				font = fonts->AddFontDefault();
			}

			ImGui::GetIO().FontDefault = font;
			fonts->Build();

			ImGui_ImplDX11_InvalidateDeviceObjects();

			((CodeEditorUI*)Get(ViewID::Code))->UpdateFont();
		}

		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
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
		static bool m_isCreateItemPopupOpened = false, m_isCreateRTOpened = false, m_isNewProjectPopupOpened = false, m_isAboutOpen = false;
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
				if (ImGui::MenuItem("New Shader")) {
					std::string file = UIHelper::GetSaveFileDialog(m_wnd->GetWindowHandle(), L"HLSL\0*.hlsl\0GLSL Vertex\0*.vert\0GLSL Pixel\0*.frag\0GLSL Geometry\0*.geom\0");
					HANDLE fileHandle = CreateFileA(file.c_str(), GENERIC_READ, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
					CloseHandle(fileHandle);
				}
				if (ImGui::MenuItem("Open", KeyboardShortcuts::Instance().GetString("Project.Open").c_str())) {
					std::string file = UIHelper::GetOpenFileDialog(m_wnd->GetWindowHandle(), L"SHADERed Project\0*.sprj\0");

					if (file.size() > 0) {
						m_data->Renderer.FlushCache();

						((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
						((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
						((PreviewUI*)Get(ViewID::Preview))->Pick(nullptr);
						((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
						((PipelineUI*)Get(ViewID::Pipeline))->Reset();

						m_data->Parser.Open(file);
						
						std::string projName = m_data->Parser.GetOpenedFile();
						projName = projName.substr(projName.find_last_of("/\\") + 1);

						SetWindowTextA(m_wnd->GetWindowHandle(), ("SHADERed (" + projName + ")").c_str());
					}
				}
				if (ImGui::MenuItem("Save", KeyboardShortcuts::Instance().GetString("Project.Save").c_str())) {
					if (m_data->Parser.GetOpenedFile() == "")
						m_saveAsProject();
					else
						m_data->Parser.Save();
				}
				if (ImGui::MenuItem("Save As", KeyboardShortcuts::Instance().GetString("Project.SaveAs").c_str()))
					m_saveAsProject();
				if (ImGui::MenuItem("Save Preview as Image", KeyboardShortcuts::Instance().GetString("Preview.SaveImage").c_str()))
					m_savePreviewPopupOpened = true;
				if (ImGui::MenuItem("Open project directory"))
					ShellExecuteA(NULL, "open", m_data->Parser.GetProjectPath("").c_str(), NULL, NULL, SW_SHOWNORMAL);

				ImGui::Separator();
				if (ImGui::MenuItem("Exit", KeyboardShortcuts::Instance().GetString("Window.Exit").c_str())) {
					m_wnd->Destroy();
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
					if (ImGui::MenuItem("Pass", KeyboardShortcuts::Instance().GetString("Project.NewShaderPass").c_str())) {
						m_createUI->SetType(PipelineItem::ItemType::ShaderPass);
						m_isCreateItemPopupOpened = true;
					}
					if (ImGui::MenuItem("Texture", KeyboardShortcuts::Instance().GetString("Project.NewTexture").c_str())) {
						std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_wnd->GetWindowHandle(), L"All\0*.*\0PNG\0*.png\0JPG\0*.jpg;*.jpeg\0DDS\0*.dds\0BMP\0*.bmp\0"));
						if (!file.empty())
							m_data->Objects.CreateTexture(file);
					}
					if (ImGui::MenuItem("Cube Map", KeyboardShortcuts::Instance().GetString("Project.NewCubeMap").c_str())) {
						std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_wnd->GetWindowHandle(), L"All\0*.*\0PNG\0*.png\0JPG\0*.jpg;*.jpeg\0DDS\0*.dds\0BMP\0*.bmp\0"));
						if (!file.empty())
							m_data->Objects.CreateTexture(file, true);
					}
					if (ImGui::MenuItem("Audio", KeyboardShortcuts::Instance().GetString("Project.NewAudio").c_str())) {
						std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_wnd->GetWindowHandle(), L"All\0*.*\0WAV\0*.wav\0MP3\0*.mp3;\0FLAC\0*.flac\0OGG\0*.ogg\0MIDI\0*.midi\0"));
						if (!file.empty())
							m_data->Objects.CreateAudio(file);
					}
					if (ImGui::MenuItem("Render Texture", KeyboardShortcuts::Instance().GetString("Project.NewRenderTexture").c_str()))
						m_isCreateRTOpened = true;
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
					if (ImGui::MenuItem("Patreon")) { ShellExecute(NULL, L"open", L"https://www.patreon.com/dfranx", NULL, NULL, SW_SHOWNORMAL); }
					if (ImGui::MenuItem("PayPal")) { ShellExecute(NULL, L"open", L"https://www.paypal.me/dfranx", NULL, NULL, SW_SHOWNORMAL); }
					ImGui::EndMenu();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Tutorial")) { ShellExecute(NULL, L"open", L"https://github.com/dfranx/SHADERed/blob/master/TUTORIAL.md", NULL, NULL, SW_SHOWNORMAL); }
				if (ImGui::MenuItem("Send feedback")) { ShellExecute(NULL, L"open", L"https://www.github.com/dfranx/SHADERed/issues", NULL, NULL, SW_SHOWNORMAL); }
				if (ImGui::MenuItem("About SHADERed")) { m_isAboutOpen = true;}

				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		if (m_performanceMode)
			((PreviewUI*)Get(ViewID::Preview))->Update(delta);

		ImGui::End();

		if (!m_performanceMode) {
			for (auto view : m_views)
				if (view->Visible) {
					ImGui::SetNextWindowSizeConstraints(ImVec2(80, 80), ImVec2(m_wnd->GetSize().x, m_wnd->GetSize().y));
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
			if (ImGui::Button("link"))
				ShellExecute(NULL, L"open", L"https://www.github.com/dfranx/SHADERed", NULL, NULL, SW_SHOWNORMAL);


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
						Settings::Instance().Project.ClearColor = ml::Color(0, 0, 0, 0);

						m_data->Renderer.FlushCache();
						((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
						((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
						((PreviewUI*)Get(ViewID::Output))->Pick(nullptr);
						((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
						((PipelineUI*)Get(ViewID::Pipeline))->Reset();
						m_data->Pipeline.New(false);

						SetWindowTextA(m_wnd->GetWindowHandle(), "SHADERed");
				} else {
					m_data->Parser.SetTemplate(m_selectedTemplate);

					m_data->Renderer.FlushCache();
					((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
					((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
					((PreviewUI*)Get(ViewID::Output))->Pick(nullptr);
					((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
					((PipelineUI*)Get(ViewID::Pipeline))->Reset();
					m_data->Pipeline.New();

					m_data->Parser.SetTemplate(settings.General.StartUpTemplate);

					SetWindowTextA(m_wnd->GetWindowHandle(), ("SHADERed (" + m_selectedTemplate + ")").c_str());
				}

				bool chosen = m_saveAsProject();
				if (!chosen) {
					if (oldFile != "") {
						m_data->Renderer.FlushCache();
						((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
						((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
						((PreviewUI*)Get(ViewID::Output))->Pick(nullptr);
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
		ImGui::SetNextWindowSize(ImVec2(400, 125), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Save Preview##main_save_preview")) {
			ImGui::TextWrapped("Path: %s", m_previewSavePath.c_str());
			ImGui::SameLine();
			if (ImGui::Button("...##save_prev_path"))
				m_previewSavePath = UIHelper::GetSaveFileDialog(m_wnd->GetWindowHandle(), L"PNG\0*.png\0DDS\0*.dds\0BMP\0*.bmp\0JPEG\0*.jpg\0TIFF\0*.tiff\0GIF\0*.gif\0ICO\0*.ico\0");
			
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
				DirectX::WICCodecs codec = DirectX::WICCodecs::WIC_CODEC_PNG;
				std::string ext = m_previewSavePath.substr(m_previewSavePath.find_last_of('.')+1);
				if (ext == "bmp")
					codec = DirectX::WICCodecs::WIC_CODEC_BMP;
				else if (ext == "jpg" || ext == "jpeg")
					codec = DirectX::WICCodecs::WIC_CODEC_JPEG;
				else if (ext == "tiff" || ext == "tif")
					codec = DirectX::WICCodecs::WIC_CODEC_TIFF;
				else if (ext == "gif")
					codec = DirectX::WICCodecs::WIC_CODEC_GIF;
				else if (ext == "ico")
					codec = DirectX::WICCodecs::WIC_CODEC_ICO;

				DirectX::ScratchImage img;
				std::wstring wpath(m_previewSavePath.begin(), m_previewSavePath.end());
				
				if (m_previewSaveSize.x > 0 && m_previewSaveSize.y > 0)
					m_data->Renderer.Render(m_previewSaveSize.x, m_previewSaveSize.y);

				DirectX::CaptureTexture(m_wnd->GetDevice(), m_wnd->GetDeviceContext(), m_data->Renderer.GetRenderTexture().GetResource(), img);
				if (ext == "dds")
					DirectX::SaveToDDSFile(img.GetImages()[0], DirectX::DDS_FLAGS_NONE, wpath.c_str());
				else 
					DirectX::SaveToWICFile(img.GetImages()[0], DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(codec), wpath.c_str());
				

				//DirectX::SaveToWICFile()
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
		if (ImGui::ListBox("##optiongroups", &m_optGroup, optGroups, _ARRAYSIZE(optGroups), height))
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
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

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
	void GUIManager::m_imguiHandleEvent(const ml::Event & e)
	{
		if (ImGui::GetCurrentContext() == NULL)
			return;

		ImGuiIO& io = ImGui::GetIO();
		switch (e.Type) {
			case ml::EventType::MouseButtonPress:
			{
				int button = 0;
				if (e.MouseButton.VK == VK_LBUTTON) button = 0;
				if (e.MouseButton.VK == VK_RBUTTON) button = 1;
				if (e.MouseButton.VK == VK_MBUTTON) button = 2;
				if (!ImGui::IsAnyMouseDown() && GetCapture() == NULL)
					SetCapture(m_wnd->GetWindowHandle());
				io.MouseDown[button] = true;
			}
			break;

			case ml::EventType::MouseButtonRelease:
			{
				int button = 0;
				if (e.MouseButton.VK == VK_LBUTTON) button = 0;
				if (e.MouseButton.VK == VK_RBUTTON) button = 1;
				if (e.MouseButton.VK == VK_MBUTTON) button = 2;
				io.MouseDown[button] = false;
				if (!ImGui::IsAnyMouseDown() && GetCapture() == m_wnd->GetWindowHandle())
					ReleaseCapture();
			}
			break;

			case ml::EventType::Scroll:
				io.MouseWheel += e.MouseWheel.Delta;
			break;

			case ml::EventType::KeyPress:
				io.KeysDown[e.Keyboard.VK] = 1;
			break;

			case ml::EventType::KeyRelease:
				io.KeysDown[e.Keyboard.VK] = 0;
			break;

			case ml::EventType::TextEnter:
				io.AddInputCharacter(e.TextCode);
			break;
		}
	}
	bool GUIManager::m_saveAsProject()
	{
		OPENFILENAME dialog;
		TCHAR filePath[MAX_PATH] = { 0 };

		ZeroMemory(&dialog, sizeof(dialog));
		dialog.lStructSize = sizeof(dialog);
		dialog.hwndOwner = m_data->GetOwner()->GetWindowHandle();
		dialog.lpstrFile = filePath;
		dialog.nMaxFile = sizeof(filePath);
		dialog.lpstrFilter = L"SHADERed Project\0*.sprj\0";
		dialog.nFilterIndex = 1;
		dialog.lpstrDefExt = L".sprj";
		dialog.Flags = OFN_PATHMUSTEXIST | OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
		bool ret = false;

		if ((ret = GetSaveFileName(&dialog)) == TRUE) {
			std::wstring wfile = std::wstring(filePath);
			std::string file(wfile.begin(), wfile.end());

			m_data->Parser.SaveAs(file, true);


			m_data->Renderer.FlushCache();

			((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
			((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
			((PreviewUI*)Get(ViewID::Preview))->Pick(nullptr);
			((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
			((PipelineUI*)Get(ViewID::Pipeline))->Reset();

			m_data->Parser.Open(file);

			std::string projName = m_data->Parser.GetOpenedFile();
			projName = projName.substr(projName.find_last_of("/\\") + 1);

			SetWindowTextA(m_wnd->GetWindowHandle(), ("SHADERed (" + projName + ")").c_str());
		}

		return ret;
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
				m_saveAsProject();
			else
				m_data->Parser.Save();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.SaveAs", [=]() {
			m_saveAsProject();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.Open", [=]() {
			m_data->Renderer.FlushCache();
			std::string file = UIHelper::GetOpenFileDialog(m_wnd->GetWindowHandle(), L"SHADERed Project\0*.sprj\0");

			if (file.size() > 0) {
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
			m_isCreateRTOpened = true;
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewCubeMap", [=]() {
			std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_wnd->GetWindowHandle(), L"All\0*.*\0PNG\0*.png\0JPG\0*.jpg;*.jpeg\0DDS\0*.dds\0BMP\0*.bmp\0"));
			if (!file.empty())
				m_data->Objects.CreateTexture(file, true);
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewTexture", [=]() {
			std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_wnd->GetWindowHandle(), L"All\0*.*\0PNG\0*.png\0JPG\0*.jpg;*.jpeg\0DDS\0*.dds\0BMP\0*.bmp\0"));
			if (!file.empty())
				m_data->Objects.CreateTexture(file);
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewAudio", [=]() {
			std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_wnd->GetWindowHandle(), L"All\0*.*\0WAV\0*.wav\0MP3\0*.mp3;\0FLAC\0*.flac\0OGG\0*.ogg\0MIDI\0*.midi\0"));
			if (!file.empty())
				m_data->Objects.CreateAudio(file);
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewShaderPass", [=]() {
			m_createUI->SetType(PipelineItem::ItemType::ShaderPass);
			m_isCreateItemPopupOpened = true;
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
		KeyboardShortcuts::Instance().SetCallback("Workspace.Help", [=]() {
			ShellExecute(NULL, L"open", L"https://github.com/dfranx/SHADERed/blob/master/TUTORIAL.md", NULL, NULL, SW_SHOWNORMAL);
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
			m_wnd->Destroy();
		});
	}
	void GUIManager::m_loadTemplateList()
	{
		m_selectedTemplate = "";

		WIN32_FIND_DATA data;
		HANDLE hFind = FindFirstFile(L".\\templates\\*", &data);      // DIRECTORY

		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					std::wstring wfile(data.cFileName);
					std::string file(wfile.begin(), wfile.end());

					if (file[0] != '.') {
						m_templates.push_back(file);
						
						if (file == Settings::Instance().General.StartUpTemplate)
							m_selectedTemplate = file;
					}
				}
			} while (FindNextFile(hFind, &data));
			FindClose(hFind);
		}

		if (m_selectedTemplate == "" && m_selectedTemplate.size() > 0)
			m_selectedTemplate = m_templates[0];

		m_data->Parser.SetTemplate(m_selectedTemplate);
	}
}