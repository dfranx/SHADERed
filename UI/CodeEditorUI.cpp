#include "CodeEditorUI.h"
#include "../ImGUI/imgui.h"
#include "../Objects/Names.h"

#include <fstream>

namespace ed
{
	void CodeEditorUI::OnEvent(const ml::Event & e)
	{}
	void CodeEditorUI::Update(float delta)
	{
		if (m_editor.size() == 0)
			return;
		
		// dock space
		for (int i = 0; i < m_editor.size(); i++) {
			if (m_editorOpen[i]) {
				ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
				if (ImGui::Begin(m_files[i].c_str(), &m_editorOpen[i], ImGuiWindowFlags_None))
					m_editor[i].Render(m_files[i].c_str());
				ImGui::End();
			}
		}

		// delete not needed editors
		for (int i = 0; i < m_editorOpen.size(); i++) {
			if (!m_editorOpen[i]) {
				m_files.erase(m_files.begin() + i);
				m_editor.erase(m_editor.begin() + i);
				m_editorOpen.erase(m_editorOpen.begin() + i);
				i--;
			}
		}
	}
	void CodeEditorUI::Open(const std::string & str)
	{
		// check if already opened
		int fileCount = std::count(m_files.begin(), m_files.end(), str);
		if (fileCount != 0)
			return;

		m_files.push_back(str);
		m_editor.push_back(TextEditor());
		m_editorOpen.push_back(true);

		TextEditor* editor = &m_editor[m_editor.size() - 1];

		TextEditor::LanguageDefinition lang = TextEditor::LanguageDefinition::HLSL();
		TextEditor::Palette pallete = TextEditor::GetDarkPalette();

		pallete[(int)TextEditor::PaletteIndex::Background] = 0x00000000;

		editor->SetPalette(pallete);
		editor->SetLanguageDefinition(lang);

		std::ifstream file(str);
		if (file.is_open()) {
			std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			editor->SetText(str);
		}
	}
}