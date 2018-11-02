#include "CodeEditorUI.h"
#include "../ImGUI/imgui.h"
#include "../Objects/Names.h"

#include <fstream>

namespace ed
{
	void CodeEditorUI::OnEvent(const ml::Event & e)
	{
		if (m_selectedItem == -1)
			return;

		if (e.Type == ml::EventType::KeyRelease) {
			if (e.Keyboard.VK == VK_F5) {
				if (e.Keyboard.Alt) {
					// ALT+F5 -> switch between stats and code
					if (!m_stats[m_selectedItem].IsActive)
						m_fetchStats(m_selectedItem);
					else m_stats[m_selectedItem].IsActive = false;
				} else {
					// F5 -> compile shader
					m_compile(m_selectedItem);
				}
			} else if (e.Keyboard.VK == 'S') {
				if (e.Keyboard.Control) // CTRL+S -> save file
					m_save(m_selectedItem);
			}
		}
	}
	void CodeEditorUI::Update(float delta)
	{
		if (m_editor.size() == 0)
			return;
		
		m_selectedItem = -1;

		// dock space
		for (int i = 0; i < m_editor.size(); i++) {
			if (m_editorOpen[i]) {
				ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
				if (ImGui::Begin((std::string(m_items[i].Name) + "##code_view").c_str(), &m_editorOpen[i], ImGuiWindowFlags_MenuBar)) {
					
					if (ImGui::BeginMenuBar()) {
						if (ImGui::BeginMenu("File")) {
							if (ImGui::MenuItem("Save", "CTRL+S")) m_save(i);
							ImGui::EndMenu();
						}
						if (ImGui::BeginMenu("Code")) {
							if (ImGui::MenuItem("Compile", "F5")) m_compile(i);
							if (!m_stats[i].IsActive && ImGui::MenuItem("Stats", "ALT+F5")) m_fetchStats(i);
							if (m_stats[i].IsActive && ImGui::MenuItem("Code", "ALT+F5")) m_stats[i].IsActive = false;
							ImGui::Separator();
							if (ImGui::MenuItem("Undo", "CTRL+Z", nullptr, m_editor[i].CanUndo())) m_editor[i].Undo();
							if (ImGui::MenuItem("Redo", "CTRL+Y", nullptr, m_editor[i].CanRedo())) m_editor[i].Redo();
							ImGui::Separator();
							if (ImGui::MenuItem("Cut", "CTRL+X")) m_editor[i].Cut();
							if (ImGui::MenuItem("Copy", "CTRL+C")) m_editor[i].Copy();
							if (ImGui::MenuItem("Paste", "CTRL+V")) m_editor[i].Paste();
							if (ImGui::MenuItem("Select All", "CTRL+A")) m_editor[i].SelectAll();

							ImGui::EndMenu();
						}

						ImGui::EndMenuBar();
					}

					if (m_stats[i].IsActive)
						m_renderStats(i);
					else {
						ImGui::PushFont(m_consolas);
						m_editor[i].Render(m_items[i].Name);
						ImGui::PopFont();
					}

					if (m_editor[i].IsFocused())
						m_selectedItem = i;
				}
				ImGui::End();
			}
		}

		// delete not needed editors
		for (int i = 0; i < m_editorOpen.size(); i++) {
			if (!m_editorOpen[i]) {
				m_items.erase(m_items.begin() + i);
				m_editor.erase(m_editor.begin() + i);
				m_editorOpen.erase(m_editorOpen.begin() + i);
				m_stats.erase(m_stats.begin() + i);
				i--;
			}
		}
	}
	void CodeEditorUI::Open(PipelineManager::Item item)
	{
		// check if already opened
		int fileCount = std::count_if(m_items.begin(), m_items.end(), [&](const PipelineManager::Item& a) { return strcmp(a.Name, item.Name) == 0; });
		if (fileCount != 0)
			return;

		ed::pipe::ShaderItem* shader = reinterpret_cast<ed::pipe::ShaderItem*>(item.Data);

		m_items.push_back(item);
		m_editor.push_back(TextEditor());
		m_editorOpen.push_back(true);
		m_stats.push_back(StatsPage());

		TextEditor* editor = &m_editor[m_editor.size() - 1];
		
		TextEditor::LanguageDefinition lang = TextEditor::LanguageDefinition::HLSL();
		TextEditor::Palette pallete = TextEditor::GetDarkPalette();

		pallete[(int)TextEditor::PaletteIndex::Background] = 0x00000000;

		editor->SetPalette(pallete);
		editor->SetLanguageDefinition(lang);

		std::ifstream file(shader->FilePath);
		if (file.is_open()) {
			std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			editor->SetText(str);
			file.close();
		}
	}
	void CodeEditorUI::m_save(int id)
	{
		ed::pipe::ShaderItem* shader = reinterpret_cast<ed::pipe::ShaderItem*>(m_items[id].Data);
		std::ofstream file(shader->FilePath);
		if (file.is_open()) {
			file << m_editor[id].GetText();
			file.close();
		}
	}
	void CodeEditorUI::m_compile(int id)
	{
		m_save(id);
		m_data->Renderer.Recompile(m_items[id].Name);
	}
	void CodeEditorUI::m_fetchStats(int id)
	{
		m_stats[id].IsActive = true;

		ID3D11DeviceChild* shader;
	}
	void CodeEditorUI::m_renderStats(int id)
	{}
}