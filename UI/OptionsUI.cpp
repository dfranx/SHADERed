#include "OptionsUI.h"
#include "CodeEditorUI.h"
#include "../ImGUI/imgui.h"
#include "../Objects/Settings.h"
#include "../Objects/ThemeContainer.h"

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
			ImGui::StyleColorsDark();

			ImGuiStyle& style = ImGui::GetStyle();
			style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		}
		else if (theme == "Light")
			ImGui::StyleColorsLight();
		else
			ImGui::GetStyle() = ThemeContainer::Instance().GetUIStyle(theme);

		editor->SetTheme(Settings::Instance().GetTextEditorPalette());		
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
		/* THEME */
		ImGui::Text("Theme: "); ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if (ImGui::BeginCombo("##opt_theme", Settings::Instance().Theme.c_str())) {
			for (int i = 0; i < 2; i++)
				if (ImGui::Selectable(m_themes[i].c_str(), m_themes[i] == Settings::Instance().Theme)) {
					Settings::Instance().Theme = m_themes[i];
					ApplyTheme();
				}
			if (m_themes.size() > 2) ImGui::Separator();

			for (int i = 2; i < m_themes.size(); i++)
				if (ImGui::Selectable(m_themes[i].c_str(), m_themes[i] == Settings::Instance().Theme)) {
					Settings::Instance().Theme = m_themes[i];
					ApplyTheme();
				}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
	}
	void OptionsUI::m_renderEditor()  {}
	void OptionsUI::m_renderShortcuts() {}
	void OptionsUI::m_renderPreview() {}
}