#include "OptionsUI.h"
#include "CodeEditorUI.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include "../Objects/Settings.h"
#include "../Objects/ThemeContainer.h"
#include "../Objects/KeyboardShortcuts.h"
#include "UIHelper.h"

#include <algorithm>

namespace ed
{
	void OptionsUI::OnEvent(const ml::Event& e)
	{
		if (m_page == Page::Shortcuts && m_selectedShortcut != -1) {
			if (e.Type == ml::EventType::KeyPress) {
				m_newShortcut.Alt = e.Keyboard.Alt;
				m_newShortcut.Ctrl = e.Keyboard.Control;
				m_newShortcut.Shift = e.Keyboard.Shift;
				if (e.Keyboard.VK != VK_CONTROL && e.Keyboard.VK != VK_SHIFT && e.Keyboard.VK != VK_MENU) {
					if (m_newShortcut.Alt == true || m_newShortcut.Ctrl == true || m_newShortcut.Shift == true) {
						if (m_newShortcut.Key1 == -1)
							m_newShortcut.Key1 = e.Keyboard.VK;
						else if (m_newShortcut.Key2 == -1)
							m_newShortcut.Key2 = e.Keyboard.VK;
						else {
							m_newShortcut.Key1 = e.Keyboard.VK;
							m_newShortcut.Key2 = -1;
						}
					}
					else {
						m_newShortcut.Key1 = -1;
						m_newShortcut.Key2 = -1;
					}
				}
				else {
					m_newShortcut.Key1 = -1;
					m_newShortcut.Key2 = -1;
				}
			}
		}
	}

	void OptionsUI::Update(float delta)
	{
		ImGui::BeginChild("##opt_container", ImVec2(0, -30));

		if (m_page == Page::General)
			m_renderGeneral();
		else if (m_page == Page::Editor)
			m_renderEditor();
		else if (m_page == Page::Shortcuts)
			m_renderShortcuts();
		else if (m_page == Page::Preview)
			m_renderPreview();
		else if (m_page == Page::Project)
			m_renderProject();

		ImGui::EndChild();
	}

	void OptionsUI::ApplyTheme()
	{
		std::string theme = Settings::Instance().Theme;
		CodeEditorUI* editor = ((CodeEditorUI*)m_ui->Get(ViewID::Code));

		if (theme == "Dark") {
			ImGuiStyle& style = ImGui::GetStyle();
			style = ImGuiStyle();

			ImGui::StyleColorsDark();

			style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		}
		else if (theme == "Light") {
			ImGuiStyle& style = ImGui::GetStyle();
			style = ImGuiStyle();

			ImGui::StyleColorsLight();
		}
		else
			ImGui::GetStyle() = ThemeContainer::Instance().GetUIStyle(theme);

		editor->SetTheme(ThemeContainer::Instance().GetTextEditorStyle(theme));
	}

	std::string OptionsUI::m_getShortcutString()
	{
		std::string ret = "";

		if (m_newShortcut.Ctrl)
			ret += "CTRL+";
		if (m_newShortcut.Alt)
			ret += "ALT+";
		if (m_newShortcut.Shift)
			ret += "SHIFT+";
		if (m_newShortcut.Key1 != -1)
			ret += ml::Keyboard::KeyToString(m_newShortcut.Key1) + "+";
		if (m_newShortcut.Key2 != -1)
			ret += ml::Keyboard::KeyToString(m_newShortcut.Key2) + "+";

		if (ret.size() == 0)
			return "";

		return ret.substr(0, ret.size() - 1);
	}

	void OptionsUI::m_loadThemeList()
	{
		m_themes.clear();
		m_themes.push_back("Dark");
		m_themes.push_back("Light");

		WIN32_FIND_DATA data;
		HANDLE hFind = FindFirstFile(L".\\themes\\*", &data);      // DIRECTORY

		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {}
				else {
					std::wstring wfile(data.cFileName);
					std::string file(wfile.begin(), wfile.end());

					m_themes.push_back(ThemeContainer::Instance().LoadTheme(file));
				}
			} while (FindNextFile(hFind, &data));
			FindClose(hFind);
		}
	}

	void OptionsUI::m_renderGeneral()
	{
		Settings* settings = &Settings::Instance();

		/* VSYNC: */
		ImGui::Text("VSync: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##optg_vsync", &settings->General.VSync))
			m_data->GetOwner()->SetVSync(settings->General.VSync);

		/* THEME */
		ImGui::Text("Theme: "); ImGui::SameLine();
		ImGui::PushItemWidth(-80);
		if (ImGui::BeginCombo("##optg_theme", settings->Theme.c_str())) {
			for (int i = 0; i < 2; i++)
				if (ImGui::Selectable(m_themes[i].c_str(), m_themes[i] == settings->Theme)) {
					settings->Theme = m_themes[i];
					ApplyTheme();
				}
			if (m_themes.size() > 2) ImGui::Separator();

			for (int i = 2; i < m_themes.size(); i++)
				if (ImGui::Selectable(m_themes[i].c_str(), m_themes[i] == settings->Theme)) {
					settings->Theme = m_themes[i];
					ApplyTheme();
				}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("REFRESH", ImVec2(-1, 0))) {
			m_loadThemeList();
			ApplyTheme();
		}



		/* AUTO ERROR SHOW: */
		ImGui::Text("Show error list when build finishes with an error: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_autoerror", &settings->General.AutoOpenErrorWindow);

		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

		/* RECOVERY: */
		ImGui::Text("Save recovery file every 10mins: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_recovery", &settings->General.Recovery);

		/* CHECK FOR UPDATES: */
		ImGui::Text("Check for updates on startup: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_checkupdates", &settings->General.CheckUpdates);

		/* GLSL: */
		ImGui::Text("Allow GLSL shaders: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_glsl", &settings->General.SupportGLSL);

		ImGui::PopStyleVar();
		ImGui::PopItemFlag();

		/* REOPEN: */
		ImGui::Text("Reopen shaders after openning a project: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_reopen", &settings->General.ReopenShaders);

		/* SHADER PASS DOUBLE CLICK: */
		ImGui::Text("Open shaders on double click on shader pass: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_opensdblclk", &settings->General.OpenShadersOnDblClk);

		/* STARTUP TEMPLATE: */
		ImGui::Text("Default template: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-80);
		if (ImGui::BeginCombo("##optg_template", settings->General.StartUpTemplate.c_str())) {
			WIN32_FIND_DATA data;
			HANDLE hFind = FindFirstFile(L".\\templates\\*", &data);      // DIRECTORY

			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
						std::wstring wfile(data.cFileName);
						std::string file(wfile.begin(), wfile.end());

						if (file[0] != '.' && ImGui::Selectable(file.c_str(), file == settings->General.StartUpTemplate))
							settings->General.StartUpTemplate = file;
					}
				} while (FindNextFile(hFind, &data));
				FindClose(hFind);
			}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();


		/* WORKSPACE STUFF*/
		ImGui::Separator();

		/* USE CUSTOM FONT: */
		ImGui::Text("Custom font: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optw_customfont", &settings->General.CustomFont);

		/* FONT: */
		ImGui::Text("Font: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-80);
		ImGui::InputText("##optw_font", settings->General.Font, 256);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("...", ImVec2(-1, 0))) {
			std::string file = UIHelper::GetOpenFileDialog(m_data->GetOwner()->GetWindowHandle(), L"Font\0*.ttf;*.otf\0");
			if (file.size() != 0)
				strcpy(settings->General.Font, file.c_str());
		}


		/* FONT SIZE: */
		ImGui::Text("Font size: ");
		ImGui::SameLine();
		if (ImGui::InputInt("##optw_fontsize", &settings->General.FontSize, 1, 5))
			settings->General.FontSize = std::max<int>(std::min<int>(settings->General.FontSize, 72), 9);

	}
	void OptionsUI::m_renderEditor()
	{
		Settings* settings = &Settings::Instance();


		/* SMART PREDICTIONS: */
		ImGui::Text("Smart predictions: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_smart_pred", &settings->Editor.SmartPredictions);

		/* SHOW WHITESPACE: */
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

		ImGui::Text("Show whitespace: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_show_whitespace", &settings->Editor.ShowWhitespace);

		ImGui::PopStyleVar();
		ImGui::PopItemFlag();

		/* FONT: */
		ImGui::Text("Font: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-80);
		ImGui::InputText("##opte_font", settings->Editor.Font, 256);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("...", ImVec2(-1, 0))) {
			std::string file = UIHelper::GetOpenFileDialog(m_data->GetOwner()->GetWindowHandle(), L"Font\0*.ttf;*.otf\0");
			if (file.size() != 0)
				strcpy(settings->Editor.Font, file.c_str());
		}


		/* FONT SIZE: */
		ImGui::Text("Font size: ");
		ImGui::SameLine();
		if (ImGui::InputInt("##opte_fontsize", &settings->Editor.FontSize, 1, 5))
			settings->Editor.FontSize = std::max<int>(std::min<int>(settings->Editor.FontSize, 72), 9);

		/* AUTO BRACE COMPLETION: */
		ImGui::Text("Brace completion: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_autobrace", &settings->Editor.AutoBraceCompletion);

		/* LINE NUMBERS: */
		ImGui::Text("Line numbers: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_ln_numbers", &settings->Editor.LineNumbers);

		/* STATUS BAR: */
		ImGui::Text("Status bar: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_statusbar", &settings->Editor.StatusBar);

		/* HORIZONTAL SCROLL BAR: */
		ImGui::Text("Enable horizontal scroll bar: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_horizscroll", &settings->Editor.HorizontalScroll);

		/* HIGHLIGHT CURRENT LINE: */
		ImGui::Text("Highlight current line: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_highlight_cur_ln", &settings->Editor.HiglightCurrentLine);

		/* SMART INDENTING: */
		ImGui::Text("Smart indenting: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_smartindent", &settings->Editor.SmartIndent);

		/* INSERT SPACES: */
		ImGui::Text("Insert spaces on tab press: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_tabspace", &settings->Editor.InsertSpaces);

		/* TAB SIZE: */
		ImGui::Text("Tab size: ");
		ImGui::SameLine();
		if (ImGui::InputInt("##opte_tabsize", &settings->Editor.TabSize, 1, 2))
			settings->Editor.TabSize = std::max<int>(std::min<int>(settings->Editor.TabSize, 12), 1);
	}
	void OptionsUI::m_renderShortcuts()
	{
		std::vector<std::string> names = KeyboardShortcuts::Instance().GetNameList();

		ImGui::Columns(2);

		for (int i = 0; i < names.size(); i++) {
			ImGui::Text(names[i].c_str());
			ImGui::NextColumn();

			if (m_selectedShortcut == i) {
				std::string txt = m_getShortcutString();
				if (txt.size() == 0)
					txt = "-- press keys --";

				ImGui::Text(txt.c_str());
				ImGui::SameLine();
				if (ImGui::Button("ASSIGN")) {
					bool updated = KeyboardShortcuts::Instance().Set(names[i], m_newShortcut.Key1, m_newShortcut.Key2, m_newShortcut.Alt, m_newShortcut.Ctrl, m_newShortcut.Shift);
					if (!updated)
						KeyboardShortcuts::Instance().Remove(names[i]);
					m_selectedShortcut = -1;
				}
				ImGui::SameLine();
				if (ImGui::Button("CANCEL"))
					m_selectedShortcut = -1;
			}
			else {
				if (ImGui::Button(KeyboardShortcuts::Instance().GetString(names[i]).c_str(), ImVec2(-1, 0))) {
					m_selectedShortcut = i;
					m_newShortcut.Ctrl = m_newShortcut.Alt = m_newShortcut.Shift = false;
					m_newShortcut.Key1 = m_newShortcut.Key2 = -1;
				}
			}

			ImGui::NextColumn();

			ImGui::Separator();
		}

		ImGui::Columns(1);
	}
	void OptionsUI::m_renderPreview()
	{
		Settings* settings = &Settings::Instance();

		/* STATUS BAR: */
		ImGui::Text("Status bar: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_status_bar", &settings->Preview.StatusBar);

		/* SHOW GIZMO: */
		ImGui::Text("Show gizmo/3d manipulators: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_gizmo_pick", &settings->Preview.Gizmo);

		/* PROP OPEN PICKED: */
		ImGui::Text("Open picked item in property window: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_prop_pick", &settings->Preview.PropertyPick);

		/* FPS LIMITI: */
		ImGui::Text("FPS limit: ");
		ImGui::SameLine();
		ImGui::InputInt("##opte_fpslimit", &settings->Preview.FPSLimit, 1, 10);
	}
	void OptionsUI::m_renderProject()
	{
		Settings* settings = &Settings::Instance();

		/* GLSL VS EXTENSION: */
		ImGui::Text("GLSL vertex shader extension: ");
		ImGui::SameLine(); ImGui::Text("."); ImGui::SameLine();
		ImGui::InputText("##optpr_glslvsext", settings->Project.GLSLVS_Extenstion, 12);

		/* GLSL PS EXTENSION: */
		ImGui::Text("GLSL pixel shader extension: ");
		ImGui::SameLine(); ImGui::Text("."); ImGui::SameLine();
		ImGui::InputText("##optpr_glslpsext", settings->Project.GLSLPS_Extenstion, 12);

		/* GLSL GS EXTENSION: */
		ImGui::Text("GLSL geometry shader extension: ");
		ImGui::SameLine(); ImGui::Text("."); ImGui::SameLine();
		ImGui::InputText("##optpr_glslgsext", settings->Project.GLSLGS_Extenstion, 12);

		/* FPS CAMERA: */
		ImGui::Text("First person camera: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optpr_fpcamera", &settings->Project.FPCamera);
	}
}