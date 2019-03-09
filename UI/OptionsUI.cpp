#include "OptionsUI.h"
#include "CodeEditorUI.h"
#include "../ImGUI/imgui.h"
#include "../Objects/Settings.h"
#include "../ImGUI/imgui_internal.h"
#include "../Objects/ThemeContainer.h"

#include <algorithm>

namespace ed
{
	void OptionsUI::OnEvent(const ml::Event& e)
	{

	}

	void OptionsUI::Update(float delta)
	{
		if (m_page == Page::General)
			m_renderGeneral();
		else if (m_page == Page::Editor)
			m_renderEditor();
		else if (m_page == Page::Shortcuts)
			m_renderShortcuts();
		else if (m_page == Page::Preview)
			m_renderPreview();
	}

	void OptionsUI::ApplyTheme()
	{
		std::string theme = Settings::Instance().Theme;
		CodeEditorUI* editor = ((CodeEditorUI*)m_ui->Get("Code"));

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



		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);


		/* AUTO ERROR SHOW: */
		ImGui::Text("Show error list when build finishes with an error: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_autoerror", &settings->General.AutoOpenErrorWindow);

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

		/* STAY ON TOP: */
		ImGui::Text("Undocked windows will stay on top: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_stayontop", &settings->General.StayOnTop);

		/* REOPEN: */
		ImGui::Text("Reopen shaders after openning a project: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_reopen", &settings->General.ReopenShaders);



		ImGui::PopStyleVar();
		ImGui::PopItemFlag();
	}
	void OptionsUI::m_renderEditor()  {
		Settings* settings = &Settings::Instance();


		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);



		/* SMART PREDICTIONS: */
		ImGui::Text("Smart predictions: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_smart_pred", &settings->Editor.SmartPredictions);

		/* FONT: */
		ImGui::Text("Font: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-80);
		if (ImGui::BeginCombo("##opte_font", settings->Editor.Font.c_str()))
			ImGui::EndCombo();

		/* SHOW WHITESPACE: */
		ImGui::Text("Show whitespace: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_show_whitespace", &settings->Editor.ShowWhitespace);

		/* AUTO BRACE COMPLETION: */
		ImGui::Text("Brace completion: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_autobrace", &settings->Editor.AutoBraceCompletion);

		ImGui::PopStyleVar();
		ImGui::PopItemFlag();

		/* LINE NUMBERS: */
		ImGui::Text("Line numbers: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_ln_numbers", &settings->Editor.LineNumbers);

		/* HIGHLIGHT CURRENT LINE: */
		ImGui::Text("Highlight current line: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_highlight_cur_ln", &settings->Editor.HiglightCurrentLine);

		/* HLSL EXTENSION: */
		ImGui::Text("HLSL extension: ");
		ImGui::SameLine(); ImGui::Text("."); ImGui::SameLine();
		ImGui::InputText("##opte_hlslext", settings->Editor.HLSL_Extenstion, 12);

		/* GLSL VS EXTENSION: */
		ImGui::Text("GLSL vertex shader extension: ");
		ImGui::SameLine(); ImGui::Text("."); ImGui::SameLine();
		ImGui::InputText("##opte_glslvsext", settings->Editor.GLSLVS_Extenstion, 12);

		/* GLSL PS EXTENSION: */
		ImGui::Text("GLSL pixel shader extension: ");
		ImGui::SameLine(); ImGui::Text("."); ImGui::SameLine();
		ImGui::InputText("##opte_glslpsext", settings->Editor.GLSLPS_Extenstion, 12);

		/* GLSL GS EXTENSION: */
		ImGui::Text("GLSL geometry shader extension: ");
		ImGui::SameLine(); ImGui::Text("."); ImGui::SameLine();
		ImGui::InputText("##opte_glslgsext", settings->Editor.GLSLGS_Extenstion, 12);

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
	void OptionsUI::m_renderShortcuts() {}
	void OptionsUI::m_renderPreview() {
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

	}
}