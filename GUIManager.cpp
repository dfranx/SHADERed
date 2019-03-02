#include "GUIManager.h"
#include "ImGUI/imgui.h"
#include "ImGUI/imgui_impl_win32.h"
#include "ImGUI/imgui_impl_dx11.h"
#include "InterfaceManager.h"
#include "UI/CreateItemUI.h"
#include "UI/CodeEditorUI.h"
#include "UI/ObjectListUI.h"
#include "UI/ErrorListUI.h"
#include "UI/PipelineUI.h"
#include "UI/PropertyUI.h"
#include "UI/PreviewUI.h"
#include "UI/OptionsUI.h"
#include "UI/PinnedUI.h"
#include "UI/UIHelper.h"
#include "Objects/Names.h"
#include "Objects/Settings.h"

#include <Windows.h>
#include <fstream>

namespace ed
{
	GUIManager::GUIManager(ed::InterfaceManager* objects, ml::Window* wnd)
	{
		m_data = objects;
		m_wnd = wnd;

		// Initialize imgui
		ImGui::CreateContext();
		
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontDefault();
		io.IniFilename = IMGUI_INI_FILE;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
		io.ConfigDockingWithShift = false;

		ImGui_ImplWin32_Init(m_wnd->GetWindowHandle());
		ImGui_ImplDX11_Init(m_wnd->GetDevice(), m_wnd->GetDeviceContext());
		
		ImGui::StyleColorsDark();

		m_views.push_back(new PipelineUI(this, objects, "Pipeline"));
		m_views.push_back(new PreviewUI(this, objects, "Preview"));
		m_views.push_back(new PropertyUI(this, objects, "Properties"));
		m_views.push_back(new PinnedUI(this, objects, "Pinned"));
		m_views.push_back(new CodeEditorUI(this, objects, "Code"));
		m_views.push_back(new ErrorListUI(this, objects, "Error List"));
		m_views.push_back(new ObjectListUI(this, objects, "Objects"));

		m_options = new OptionsUI(this, objects, "Options");
		m_createUI = new CreateItemUI(this, objects);

		((OptionsUI*)m_options)->SetGroup(OptionsUI::Page::General);
	}
	GUIManager::~GUIManager()
	{
		for (auto view : m_views)
			delete view;

		delete m_createUI;

		// release memory
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void GUIManager::OnEvent(const ml::Event& e)
	{
		m_imguiHandleEvent(e);
		
		for (auto view : m_views)
			view->OnEvent(e);
	}
	void GUIManager::Update(float delta)
	{
		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// window flags
		const ImGuiWindowFlags window_flags = (ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		// create window
		ImGui::Begin("DockSpace Demo", nullptr, window_flags);
		ImGui::PopStyleVar(3);


		// dockspace
		ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruDockspace | ImGuiDockNodeFlags_None);

		// menu
		static bool s_isCreateItemPopupOpened = false, s_isCreateRTOpened = false;
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New")) {
					m_data->Renderer.FlushCache();
					((CodeEditorUI*)Get("Code"))->CloseAll();
					((PinnedUI*)Get("Pinned"))->CloseAll();
					((PreviewUI*)Get("Preview"))->Pick(nullptr);
					m_data->Pipeline.New();
				}
				if (ImGui::MenuItem("Open")) {
					m_data->Renderer.FlushCache();
					std::string file = UIHelper::GetOpenFileDialog(m_wnd->GetWindowHandle(), L"SHADERed Project\0*.sprj\0");

					if (file.size() > 0) {
						((CodeEditorUI*)Get("Code"))->CloseAll();
						((PinnedUI*)Get("Pinned"))->CloseAll();
						((PreviewUI*)Get("Preview"))->Pick(nullptr);

						m_data->Parser.Open(file);
					}
				}
				if (ImGui::MenuItem("Save")) {
					if (m_data->Parser.GetOpenedFile() == "")
						m_saveAsProject();
					else
						m_data->Parser.Save();
				}
				if (ImGui::MenuItem("Save As"))
					m_saveAsProject();
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Project")) {
				if (ImGui::MenuItem("Rebuild project")) {
					std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
					for (PipelineItem*& pass : passes)
						m_data->Renderer.Recompile(pass->Name);
				}
				if (ImGui::BeginMenu("Create")) {
					if (ImGui::MenuItem("Pass")) {
						m_createUI->SetType(PipelineItem::ItemType::ShaderPass);
						s_isCreateItemPopupOpened = true;
					}
					if (ImGui::MenuItem("Texture")) {
						std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_wnd->GetWindowHandle(), L"All\0*.*\0PNG\0*.png\0JPG\0*.jpg;*.jpeg\0DDS\0*.dds\0BMP\0*.bmp\0"));
						if (!file.empty())
							m_data->Objects.CreateTexture(file);
					}
					if (ImGui::MenuItem("Cube Map")) {
						std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_wnd->GetWindowHandle(), L"All\0*.*\0PNG\0*.png\0JPG\0*.jpg;*.jpeg\0DDS\0*.dds\0BMP\0*.bmp\0"));
						if (!file.empty())
							m_data->Objects.CreateTexture(file, true);
					}
					if (ImGui::MenuItem("Render Texture"))
						s_isCreateRTOpened = true;
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
				if (ImGui::MenuItem("Options")) { 
					m_optionsOpened = true;
				}

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help")) {
				if (ImGui::MenuItem("Support Us")) { /* TODO */ }
				ImGui::Separator();
				if (ImGui::MenuItem("View Help")) { /* TODO */ }
				if (ImGui::MenuItem("Tips")) { /* TODO */ }
				if (ImGui::MenuItem("Send feedback")) { /* TODO */ }
				if (ImGui::MenuItem("About SHADERed")) { /* TODO */ }

				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}


		for (auto view : m_views)
			if (view->Visible) {
				ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSizeConstraints(ImVec2(50, 50), ImVec2(m_wnd->GetSize().x, m_wnd->GetSize().y));
				if (ImGui::Begin(view->Name.c_str(), &view->Visible)) view->Update(delta);
				ImGui::End();
			}

		Get("Code")->Update(delta);


		if (m_optionsOpened) {
			ImGui::Begin("Options", &m_optionsOpened);
			m_renderOptions();
			ImGui::End();
		}

		if (s_isCreateItemPopupOpened) {
			ImGui::OpenPopup("Create Item##main_create_item");
			s_isCreateItemPopupOpened = false;
		}

		if (s_isCreateRTOpened) {
			ImGui::OpenPopup("Create RT##main_create_rt");
			s_isCreateRTOpened = false;
		}

		// Create Item popup
		ImGui::SetNextWindowSize(ImVec2(430, 175), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Create Item##main_create_item")) {
			m_createUI->Update(delta);

			if (ImGui::Button("Ok")) {
				if (m_createUI->Create())
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create RT popup
		ImGui::SetNextWindowSize(ImVec2(430, 175), ImGuiCond_Once);
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

		ImGui::End();


		// render ImGUI
		ImGui::Render();
	}
	void GUIManager::m_renderOptions()
	{
		OptionsUI* options = (OptionsUI*)m_options;
		static const char* optGroups[4] = {
			"General",
			"Editor",
			"Shortcuts",
			"Preview"
		};

		float height = abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y - ImGui::GetStyle().WindowPadding.y*2) / ImGui::GetTextLineHeightWithSpacing() - 1;

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, 100 + ImGui::GetStyle().WindowPadding.x * 2);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(100);
		if (ImGui::ListBox("##optiongroups", &m_optGroup, optGroups, _ARRAYSIZE(optGroups), height))
			options->SetGroup((OptionsUI::Page)m_optGroup);
		ImGui::PopStyleColor();

		ImGui::NextColumn();

		options->Update(0.0f);

		ImGui::Columns();

		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 160);
		if (ImGui::Button("Save", ImVec2(70, 0)))
			Settings::Instance().Save();

		ImGui::SameLine();

		ImGui::SetCursorPosX(ImGui::GetWindowWidth()-80);
		if (ImGui::Button("Close", ImVec2(-1, 0))) { m_optionsOpened = false; }


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
	UIView * GUIManager::Get(const std::string & name)
	{
		for (auto view : m_views)
			if (view->Name == name)
				return view;
		return nullptr;
	}
	void GUIManager::SaveSettings()
	{
		std::ofstream data("gui.dat");

		for (auto view : m_views)
			data.put(view->Visible);

		data.close();
	}
	void GUIManager::LoadSettings()
	{
		std::ifstream data("gui.dat");

		if (data.is_open()) {
			for (auto view : m_views)
				view->Visible = data.get();

			data.close();
		}

		Get("Code")->Visible = false;

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
	void GUIManager::m_saveAsProject()
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

		if (GetSaveFileName(&dialog) == TRUE) {
			std::wstring wfile = std::wstring(filePath);
			std::string file(wfile.begin(), wfile.end());

			m_data->Parser.SaveAs(file);
		}
	}
}