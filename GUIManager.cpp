#include "GUIManager.h"
#include "ImGUI/imgui.h"
#include "ImGUI/imgui_dock.h"
#include "ImGUI/imgui_impl_win32.h"
#include "ImGUI/imgui_impl_dx11.h"
#include "UI/PipelineUI.h"
#include "UI/PropertyUI.h"
#include "UI/PreviewUI.h"
#include "UI/PinnedUI.h"

#include <Windows.h>
#include <fstream>

namespace ed
{
	GUIManager::GUIManager(ed::InterfaceManager* objects, ml::Window* wnd)
	{
		m_views.push_back(new PipelineUI(this, objects, "Pipeline"));
		m_views.push_back(new PreviewUI(this, objects, "Preview"));
		m_views.push_back(new PropertyUI(this, objects, "Properties"));
		m_views.push_back(new PinnedUI(this, objects, "Pinned"));

		m_applySize = false;

		m_data = objects;
		m_wnd = wnd;

		// Initialize imgui
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange;
		ImGui_ImplWin32_Init(m_wnd->GetWindowHandle());
		ImGui_ImplDX11_Init(m_wnd->GetDevice(), m_wnd->GetDeviceContext());
		ImGui::StyleColorsDark();
		ImGui::InitDock();

	}
	GUIManager::~GUIManager()
	{
		for (auto view : m_views)
			delete view;

		// release memory
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void GUIManager::OnEvent(const ml::Event& e)
	{
		m_imguiHandleEvent(e);

		if (e.Type == ml::EventType::WindowResize) {
			m_applySize = true;
		}

		for (auto view : m_views)
			view->OnEvent(e);
	}
	void GUIManager::Update(float delta)
	{
		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// main window
		const ImGuiWindowFlags flags = (ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
		
		ImGui::SetNextWindowPos(ImVec2(0, 0));

		if (!m_applySize) 
			ImGui::SetNextWindowSize(ImVec2(m_wnd->GetSize().x, m_wnd->GetSize().y), ImGuiCond_Once);
		else {
			ImGui::SetNextWindowSize(ImVec2(m_wnd->GetSize().x, m_wnd->GetSize().y), ImGuiCond_Always);
			m_applySize = false;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
		const bool visible = ImGui::Begin("MDWindow_Invisible", NULL, ImVec2(0, 0), 0.5f, flags);
		ImGui::PopStyleVar();

		if (visible) {
			// menu
			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("File")) {
					if (ImGui::MenuItem("New")) {
						m_data->Renderer.FlushCache();
						m_data->Pipeline.New();
					}
					if (ImGui::MenuItem("Open")) {
						m_data->Renderer.FlushCache();
						m_openProject();
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
				if (ImGui::BeginMenu("Create")) {
					ImGui::MenuItem("Shader");
					ImGui::MenuItem("Geometry");
					ImGui::MenuItem("Input Layout");
					ImGui::MenuItem("Topology");
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Window")) {
					for (auto view : m_views)
						ImGui::MenuItem(view->Name.c_str(), 0, &view->Visible);
					ImGui::EndMenu();
				}
				ImGui::EndMainMenuBar();
			}



			// dock space
			ImGui::BeginDockspace();

			for (auto view : m_views)
				if (view->Visible) {
					if (ImGui::BeginDock(view->Name.c_str())) view->Update(delta);
					ImGui::EndDock();
				}

			ImGui::EndDockspace();
		}
		ImGui::End();
		

		// render ImGUI
		ImGui::Render();
	}
	void GUIManager::Render()
	{
		// actually render to back buffer
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
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
	void GUIManager::m_openProject()
	{
		OPENFILENAME dialog;
		TCHAR filePath[MAX_PATH] = { 0 };

		ZeroMemory(&dialog, sizeof(dialog));
		dialog.lStructSize = sizeof(dialog);
		dialog.hwndOwner = m_data->GetOwner()->GetWindowHandle();
		dialog.lpstrFile = filePath;
		dialog.nMaxFile = sizeof(filePath);
		dialog.lpstrFilter = L"HLSLed Project\0*.sprj\0";
		dialog.nFilterIndex = 0;
		dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
		
		if (GetOpenFileName(&dialog) == TRUE) {
			std::wstring wfile = std::wstring(filePath);
			std::string file(wfile.begin(), wfile.end());

			m_data->Parser.Open(file);
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
		dialog.lpstrFilter = L"HLSLed Project\0*.sprj\0";
		dialog.nFilterIndex = 1;
		dialog.lpstrDefExt = L".sprj";
		dialog.Flags = OFN_PATHMUSTEXIST | OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT;

		if (GetSaveFileName(&dialog) == TRUE) {
			std::wstring wfile = std::wstring(filePath);
			std::string file(wfile.begin(), wfile.end());

			m_data->Parser.SaveAs(file);
		}
	}
}