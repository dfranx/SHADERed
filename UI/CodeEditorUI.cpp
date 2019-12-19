#include "CodeEditorUI.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include "../Objects/Names.h"
#include "../Objects/Logger.h"
#include "../Objects/Settings.h"
#include "../Objects/ShaderTranscompiler.h"
#include "../Objects/ThemeContainer.h"
#include "../Objects/KeyboardShortcuts.h"

#include <iostream>
#include <fstream>

#if defined(_WIN32)
	#include <windows.h>
#elif defined(__linux__) || defined(__unix__)
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/inotify.h>
	#define EVENT_SIZE  ( sizeof (struct inotify_event) )
	#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
#endif

#define STATUSBAR_HEIGHT 20 * Settings::Instance().DPIScale

namespace ed
{
	CodeEditorUI::~CodeEditorUI() {
		SetAutoRecompile(false);
		SetTrackFileChanges(false);
	}
	void CodeEditorUI::m_setupShortcuts() {
		KeyboardShortcuts::Instance().SetCallback("CodeUI.Compile", [=]() {
			if (m_selectedItem == -1)
				return;

			m_compile(m_selectedItem);
		});
		KeyboardShortcuts::Instance().SetCallback("CodeUI.Save", [=]() {
			if (m_selectedItem == -1)
				return;

			m_save(m_selectedItem);
		});
		KeyboardShortcuts::Instance().SetCallback("CodeUI.SwitchView", [=]() {
			// REMOVED
		});
		KeyboardShortcuts::Instance().SetCallback("CodeUI.ToggleStatusbar", [=]() {
			Settings::Instance().Editor.StatusBar = !Settings::Instance().Editor.StatusBar;
		});
	}
	void CodeEditorUI::OnEvent(const SDL_Event& e)
	{
	}
	void CodeEditorUI::Update(float delta)
	{
		if (m_editor.size() == 0)
			return;
		
		m_selectedItem = -1;

		// counters for each shader type for window ids
		int wid[5] = { 0 }; // vs, ps, gs, cs, pl

		// code editor windows
		for (int i = 0; i < m_editor.size(); i++) {
			if (m_editorOpen[i]) {
				bool isPluginItem = m_items[i]->Type == PipelineItem::ItemType::PluginItem;
				pipe::PluginItemData* plData = (pipe::PluginItemData*)m_items[i]->Data;

				std::string shaderType = isPluginItem ? "PL" : m_shaderTypeId[i] == 0 ? "VS" : (m_shaderTypeId[i] == 1 ? "PS" : (m_shaderTypeId[i] == 2 ? "GS" : "CS"));
				std::string windowName(std::string(m_items[i]->Name) + " (" + shaderType + ")");
				
				if (m_editor[i].IsTextChanged() && !m_data->Parser.IsProjectModified())
					m_data->Parser.ModifyProject();

				ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
				if (ImGui::Begin((std::string(windowName) + "###code_view" + shaderType + std::to_string(wid[isPluginItem ? 4 : m_shaderTypeId[i]])).c_str(), &m_editorOpen[i], (ImGuiWindowFlags_UnsavedDocument * m_editor[i].IsTextChanged()) | ImGuiWindowFlags_MenuBar)) {
					if (ImGui::BeginMenuBar()) {
						if (ImGui::BeginMenu("File")) {
							if (ImGui::MenuItem("Save", KeyboardShortcuts::Instance().GetString("CodeUI.Save").c_str())) m_save(i);
							ImGui::EndMenu();
						}
						if (ImGui::BeginMenu("Code")) {
							if (ImGui::MenuItem("Compile", KeyboardShortcuts::Instance().GetString("CodeUI.Compile").c_str())) m_compile(i);

							if (!m_stats[i].IsActive && ImGui::MenuItem("Stats", KeyboardShortcuts::Instance().GetString("CodeUI.SwitchView").c_str(), nullptr, false)) m_stats[i].Fetch(m_items[i], m_editor[i].GetText(), m_shaderTypeId[i]);
							
							if (m_stats[i].IsActive && ImGui::MenuItem("Code", KeyboardShortcuts::Instance().GetString("CodeUI.SwitchView").c_str())) m_stats[i].IsActive = false;
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
						m_stats[i].Render();
					else {
						// add error markers if needed
						auto msgs = m_data->Messages.GetMessages();
						int groupMsg = 0;
						TextEditor::ErrorMarkers groupErrs;
						for (int j = 0; j < msgs.size(); j++)
							if (msgs[j].Line > 0 && msgs[j].Group == m_items[i]->Name && msgs[j].Shader == m_shaderTypeId[i])
								groupErrs[msgs[j].Line] = msgs[j].Text;
						m_editor[i].SetErrorMarkers(groupErrs);

						bool statusbar = Settings::Instance().Editor.StatusBar;

						// render code
						ImGui::PushFont(m_font);
						m_editor[i].Render(windowName.c_str(), ImVec2(0, -statusbar*STATUSBAR_HEIGHT));
						ImGui::PopFont();

						// status bar
						if (statusbar) {
							auto cursor = m_editor[i].GetCursorPosition();

							ImGui::Separator();
							ImGui::Text("Line %d\tCol %d\tType: %s\tPath: %s", cursor.mLine, cursor.mColumn, m_editor[i].GetLanguageDefinition().mName.c_str(), m_paths[i].c_str());
						}
					}

					if (m_editor[i].IsFocused())
						m_selectedItem = i;
				}

				if (m_focusWindow) {
					if (m_focusItem == m_items[i]->Name && m_focusSID == m_shaderTypeId[i]) {
						ImGui::SetWindowFocus();
						m_focusWindow = false;
					}
				}

				wid[isPluginItem ? 4 : m_shaderTypeId[i]]++;
				ImGui::End();
			}
		}

		// save popup
		for (int i = 0; i < m_editorOpen.size(); i++)
			if (!m_editorOpen[i] && m_editor[i].IsTextChanged()) {
				ImGui::OpenPopup("Save Changes##code_save");
				m_savePopupOpen = i;
				m_editorOpen[i] = true;
				break;
			}

		// Create Item popup
		ImGui::SetNextWindowSize(ImVec2(200, 75), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Save Changes##code_save")) {
			ImGui::Text("Do you want to save changes?");
			if (ImGui::Button("Yes")) {
				m_save(m_savePopupOpen);
				m_editorOpen[m_savePopupOpen] = false;
				m_savePopupOpen = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No")) {
				m_editorOpen[m_savePopupOpen] = false;
				m_savePopupOpen = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				m_editorOpen[m_savePopupOpen] = true;
				m_savePopupOpen = -1;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		// delete not needed editors
		if (m_savePopupOpen == -1) {
			for (int i = 0; i < m_editorOpen.size(); i++) {
				if (!m_editorOpen[i]) {
					if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[i]->Data);
						shader->Owner->CloseCodeEditorItem(m_shaderTypeId[i]);
					}

					m_items.erase(m_items.begin() + i);
					m_editor.erase(m_editor.begin() + i);
					m_editorOpen.erase(m_editorOpen.begin() + i);
					m_stats.erase(m_stats.begin() + i);
					m_paths.erase(m_paths.begin() + i);
					m_shaderTypeId.erase(m_shaderTypeId.begin() + i);
					i--;
				}
			}
		}
	}
	void CodeEditorUI::m_open(PipelineItem* item, int sid)
	{
		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			ed::pipe::ShaderPass* shader = reinterpret_cast<ed::pipe::ShaderPass*>(item->Data);

			if (Settings::Instance().General.UseExternalEditor) {
				std::string path = "";
				if (sid == 0)
					path = m_data->Parser.GetProjectPath(shader->VSPath);
				else if (sid == 1)
					path = m_data->Parser.GetProjectPath(shader->PSPath);
				else if (sid == 2)
					path = m_data->Parser.GetProjectPath(shader->GSPath);

				#if defined(__APPLE__)
					system(("open " + path).c_str());
				#elif defined(__linux__) || defined(__unix__)
					system(("xdg-open " + path).c_str());
				#elif defined(_WIN32)
					ShellExecuteA(0, 0, path.c_str(), 0, 0, SW_SHOW);
				#endif

				return;
			}

			// check if already opened
			for (int i = 0; i < m_items.size(); i++) {
				if (m_shaderTypeId[i] == sid) {
					ed::pipe::ShaderPass* sData = reinterpret_cast<ed::pipe::ShaderPass*>(m_items[i]->Data);
					bool match = false;
					if (sid == 0 && (strcmp(shader->VSPath, sData->VSPath) == 0 || strcmp(shader->VSPath, sData->PSPath) == 0 || strcmp(shader->VSPath, sData->GSPath) == 0))
						match = true;
					else if (sid == 1 && (strcmp(shader->PSPath, sData->VSPath) == 0 || strcmp(shader->PSPath, sData->PSPath) == 0 || strcmp(shader->PSPath, sData->GSPath) == 0))
						match = true;
					else if (sid == 2 && (strcmp(shader->GSPath, sData->VSPath) == 0 || strcmp(shader->GSPath, sData->PSPath) == 0 || strcmp(shader->GSPath, sData->GSPath) == 0))
						match = true;

					if (match) {
						m_focusWindow = true;
						m_focusSID = sid;
						m_focusItem = m_items[i]->Name;
						return;
					}
				}
			}

			m_items.push_back(item);
			m_editor.push_back(TextEditor());
			m_editorOpen.push_back(true);
			m_stats.push_back(StatsPage(m_data));

			TextEditor* editor = &m_editor[m_editor.size() - 1];

			TextEditor::LanguageDefinition defHLSL = TextEditor::LanguageDefinition::HLSL();
			TextEditor::LanguageDefinition defGLSL = TextEditor::LanguageDefinition::GLSL();

			editor->SetPalette(ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme));
			editor->SetTabSize(Settings::Instance().Editor.TabSize);
			editor->SetInsertSpaces(Settings::Instance().Editor.InsertSpaces);
			editor->SetSmartIndent(Settings::Instance().Editor.SmartIndent);
			editor->SetShowWhitespaces(Settings::Instance().Editor.ShowWhitespace);
			editor->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
			editor->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
			editor->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
			editor->SetHorizontalScroll(Settings::Instance().Editor.HorizontalScroll);
			editor->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
			m_loadEditorShortcuts(editor);

			bool isHLSL = false;
			if (sid == 0)
				isHLSL = ShaderTranscompiler::GetShaderTypeFromExtension(shader->VSPath) == ShaderLanguage::HLSL;
			else if (sid == 1)
				isHLSL = ShaderTranscompiler::GetShaderTypeFromExtension(shader->PSPath) == ShaderLanguage::HLSL;
			else if (sid == 2)
				isHLSL = ShaderTranscompiler::GetShaderTypeFromExtension(shader->GSPath) == ShaderLanguage::HLSL;
			editor->SetLanguageDefinition(isHLSL ? defHLSL : defGLSL);
			
			m_shaderTypeId.push_back(sid);

			std::string shaderContent = "";
			if (sid == 0) {
				shaderContent = m_data->Parser.LoadProjectFile(shader->VSPath);
				m_paths.push_back(shader->VSPath);
			}
			else if (sid == 1) {
				shaderContent = m_data->Parser.LoadProjectFile(shader->PSPath);
				m_paths.push_back(shader->PSPath);
			}
			else if (sid == 2) {
				shaderContent = m_data->Parser.LoadProjectFile(shader->GSPath);
				m_paths.push_back(shader->GSPath);
			}
			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		}
		else if (item->Type == PipelineItem::ItemType::ComputePass)
		{
			ed::pipe::ComputePass *shader = reinterpret_cast<ed::pipe::ComputePass *>(item->Data);

			if (Settings::Instance().General.UseExternalEditor)
			{
				std::string path = m_data->Parser.GetProjectPath(shader->Path);
#if defined(__APPLE__)
				system(("open " + path).c_str());
#elif defined(__linux__) || defined(__unix__)
				system(("xdg-open " + path).c_str());
#elif defined(_WIN32)
				ShellExecuteA(0, 0, path.c_str(), 0, 0, SW_SHOW);
#endif
				return;
			}

			// check if already opened
			for (int i = 0; i < m_items.size(); i++)
			{
				if (m_shaderTypeId[i] == 3 && m_items[i]->Type == PipelineItem::ItemType::ComputePass)
				{
					ed::pipe::ComputePass *sData = reinterpret_cast<ed::pipe::ComputePass *>(m_items[i]->Data);
					bool match = false;
					if (sid == 3 && strcmp(shader->Path, sData->Path) == 0)
						match = true;

					if (match)
					{
						m_focusWindow = true;
						m_focusSID = sid;
						m_focusItem = m_items[i]->Name;
						return;
					}
				}
			}

			// TODO: some of this can be moved outside of the if() {} block
			m_items.push_back(item);
			m_editor.push_back(TextEditor());
			m_editorOpen.push_back(true);
			m_stats.push_back(StatsPage(m_data));
			m_paths.push_back(shader->Path);

			TextEditor *editor = &m_editor[m_editor.size() - 1];

			TextEditor::LanguageDefinition defHLSL = TextEditor::LanguageDefinition::HLSL();
			TextEditor::LanguageDefinition defGLSL = TextEditor::LanguageDefinition::GLSL();

			editor->SetPalette(ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme));
			editor->SetTabSize(Settings::Instance().Editor.TabSize);
			editor->SetInsertSpaces(Settings::Instance().Editor.InsertSpaces);
			editor->SetSmartIndent(Settings::Instance().Editor.SmartIndent);
			editor->SetShowWhitespaces(Settings::Instance().Editor.ShowWhitespace);
			editor->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
			editor->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
			editor->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
			editor->SetHorizontalScroll(Settings::Instance().Editor.HorizontalScroll);
			editor->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
			m_loadEditorShortcuts(editor);

			bool isHLSL = ShaderTranscompiler::GetShaderTypeFromExtension(shader->Path) == ShaderLanguage::HLSL;
			editor->SetLanguageDefinition(isHLSL ? defHLSL : defGLSL);

			m_shaderTypeId.push_back(sid);

			std::string shaderContent = m_data->Parser.LoadProjectFile(shader->Path);
			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		}
		else if (item->Type == PipelineItem::ItemType::AudioPass)
		{
			ed::pipe::AudioPass *shader = reinterpret_cast<ed::pipe::AudioPass *>(item->Data);

			if (Settings::Instance().General.UseExternalEditor)
			{
				std::string path = m_data->Parser.GetProjectPath(shader->Path);
#if defined(__APPLE__)
				system(("open " + path).c_str());
#elif defined(__linux__) || defined(__unix__)
				system(("xdg-open " + path).c_str());
#elif defined(_WIN32)
				ShellExecuteA(0, 0, path.c_str(), 0, 0, SW_SHOW);
#endif
				return;
			}

			// check if already opened
			for (int i = 0; i < m_items.size(); i++)
			{
				if (m_shaderTypeId[i] == 1 && m_items[i]->Type == PipelineItem::ItemType::AudioPass)
				{
					ed::pipe::AudioPass *sData = reinterpret_cast<ed::pipe::AudioPass *>(m_items[i]->Data);
					bool match = false;
					if (sid == 1 && strcmp(shader->Path, sData->Path) == 0)
						match = true;

					if (match)
					{
						m_focusWindow = true;
						m_focusSID = sid;
						m_focusItem = m_items[i]->Name;
						return;
					}
				}
			}

			// TODO: some of this can be moved outside of the if() {} block
			m_items.push_back(item);
			m_editor.push_back(TextEditor());
			m_editorOpen.push_back(true);
			m_stats.push_back(StatsPage(m_data));
			m_paths.push_back(shader->Path);

			TextEditor *editor = &m_editor[m_editor.size() - 1];

			TextEditor::LanguageDefinition defHLSL = TextEditor::LanguageDefinition::HLSL();
			TextEditor::LanguageDefinition defGLSL = TextEditor::LanguageDefinition::GLSL();

			editor->SetPalette(ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme));
			editor->SetTabSize(Settings::Instance().Editor.TabSize);
			editor->SetInsertSpaces(Settings::Instance().Editor.InsertSpaces);
			editor->SetSmartIndent(Settings::Instance().Editor.SmartIndent);
			editor->SetShowWhitespaces(Settings::Instance().Editor.ShowWhitespace);
			editor->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
			editor->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
			editor->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
			editor->SetHorizontalScroll(Settings::Instance().Editor.HorizontalScroll);
			editor->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
			m_loadEditorShortcuts(editor);

			bool isHLSL = ShaderTranscompiler::GetShaderTypeFromExtension(shader->Path) == ShaderLanguage::HLSL;
			editor->SetLanguageDefinition(isHLSL ? defHLSL : defGLSL);

			m_shaderTypeId.push_back(sid);

			std::string shaderContent = m_data->Parser.LoadProjectFile(shader->Path);
			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		}
	}
	void CodeEditorUI::OpenPluginCode(PipelineItem* item, const char* filepath, int sid)
	{
		if (item->Type == PipelineItem::ItemType::PluginItem)
		{
			pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(item->Data);

			if (Settings::Instance().General.UseExternalEditor)
			{
				std::string path = m_data->Parser.GetProjectPath(filepath);
	#if defined(__APPLE__)
				system(("open " + path).c_str());
	#elif defined(__linux__) || defined(__unix__)
				system(("xdg-open " + path).c_str());
	#elif defined(_WIN32)
				ShellExecuteA(0, 0, path.c_str(), 0, 0, SW_SHOW);
	#endif
				return;
			}

			// check if already opened
			for (int i = 0; i < m_paths.size(); i++)
			{
				if (m_paths[i] == filepath)
				{
					m_focusWindow = true;
					m_focusSID = sid;
					m_focusItem = m_items[i]->Name;
					return;
				}
			}

			// TODO: some of this can be moved outside of the if() {} block
			m_items.push_back(item);
			m_editor.push_back(TextEditor());
			m_editorOpen.push_back(true);
			m_stats.push_back(StatsPage(m_data));
			m_paths.push_back(filepath);

			TextEditor* editor = &m_editor[m_editor.size() - 1];

			TextEditor::LanguageDefinition defPlugin = m_buildLanguageDefinition(shader->Owner, sid, shader->Type, filepath);

			editor->SetPalette(ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme));
			editor->SetTabSize(Settings::Instance().Editor.TabSize);
			editor->SetInsertSpaces(Settings::Instance().Editor.InsertSpaces);
			editor->SetSmartIndent(Settings::Instance().Editor.SmartIndent);
			editor->SetShowWhitespaces(Settings::Instance().Editor.ShowWhitespace);
			editor->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
			editor->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
			editor->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
			editor->SetHorizontalScroll(Settings::Instance().Editor.HorizontalScroll);
			editor->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
			m_loadEditorShortcuts(editor);
			
			editor->SetLanguageDefinition(defPlugin);

			m_shaderTypeId.push_back(sid);

			std::string shaderContent = m_data->Parser.LoadProjectFile(filepath);
			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		}
	}
	void CodeEditorUI::OpenVS(PipelineItem* item)
	{
		m_open(item, 0);
	}
	void CodeEditorUI::OpenPS(PipelineItem* item)
	{
		m_open(item, 1);
	}
	void CodeEditorUI::OpenGS(PipelineItem* item)
	{
		m_open(item, 2);
	}
	void CodeEditorUI::OpenCS(PipelineItem* item)
	{
		m_open(item, 3);
	}
	void CodeEditorUI::CloseAll()
	{
		// delete not needed editors
		for (int i = 0; i < m_editorOpen.size(); i++) {
			if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[i]->Data);
				shader->Owner->CloseCodeEditorItem(m_shaderTypeId[i]);
			}

			m_items.erase(m_items.begin() + i);
			m_editor.erase(m_editor.begin() + i);
			m_editorOpen.erase(m_editorOpen.begin() + i);
			m_stats.erase(m_stats.begin() + i);
			m_paths.erase(m_paths.begin() + i);
			m_shaderTypeId.erase(m_shaderTypeId.begin() + i);
			i--;
		}
	}
	void CodeEditorUI::CloseAllFrom(PipelineItem* item)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (m_items[i] == item) {
				if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[i]->Data);
					shader->Owner->CloseCodeEditorItem(m_shaderTypeId[i]);
				}

				m_items.erase(m_items.begin() + i);
				m_editor.erase(m_editor.begin() + i);
				m_editorOpen.erase(m_editorOpen.begin() + i);
				m_stats.erase(m_stats.begin() + i);
				m_paths.erase(m_paths.begin() + i);
				m_shaderTypeId.erase(m_shaderTypeId.begin() + i);
				i--;
			}
		}
	}
	void CodeEditorUI::SaveAll()
	{
		for (int i = 0; i < m_items.size(); i++)
			m_save(i);
	}
	std::vector<std::pair<std::string, int>> CodeEditorUI::GetOpenedFiles()
	{
		std::vector<std::pair<std::string, int>> ret;
		for (int i = 0; i < m_items.size(); i++)
			ret.push_back(std::make_pair(std::string(m_items[i]->Name), m_shaderTypeId[i]));
		return ret;
	}
	std::vector<std::string> CodeEditorUI::GetOpenedFilesData()
	{
		std::vector<std::string> ret;
		for (int i = 0; i < m_items.size(); i++)
			ret.push_back(m_editor[i].GetText());
		return ret;
	}
	void CodeEditorUI::SetOpenedFilesData(const std::vector<std::string>& data)
	{
		for (int i = 0; i < m_items.size() && i < data.size(); i++)
			m_editor[i].SetText(data[i]);
	}
	void CodeEditorUI::m_save(int id)
	{
		bool canSave = true;

		// prompt user to choose a project location first
		if (m_data->Parser.GetOpenedFile() == "")
			canSave = m_ui->SaveAsProject(true);
		if (!canSave)
			return;

		if (m_items[id]->Type == PipelineItem::ItemType::ShaderPass) {
			ed::pipe::ShaderPass *shader = reinterpret_cast<ed::pipe::ShaderPass *>(m_items[id]->Data);

			m_editor[id].ResetTextChanged();

			if (m_shaderTypeId[id] == 0)
				m_data->Parser.SaveProjectFile(shader->VSPath, m_editor[id].GetText());
			else if (m_shaderTypeId[id] == 1)
				m_data->Parser.SaveProjectFile(shader->PSPath, m_editor[id].GetText());
			else if (m_shaderTypeId[id] == 2)
				m_data->Parser.SaveProjectFile(shader->GSPath, m_editor[id].GetText());
		} 
		else if (m_items[id]->Type == PipelineItem::ItemType::ComputePass) {
			ed::pipe::ComputePass *shader = reinterpret_cast<ed::pipe::ComputePass *>(m_items[id]->Data);
			m_editor[id].ResetTextChanged();
			m_data->Parser.SaveProjectFile(shader->Path, m_editor[id].GetText());
		} 
		else if (m_items[id]->Type == PipelineItem::ItemType::AudioPass) {
			ed::pipe::AudioPass *shader = reinterpret_cast<ed::pipe::AudioPass *>(m_items[id]->Data);
			m_editor[id].ResetTextChanged();
			m_data->Parser.SaveProjectFile(shader->Path, m_editor[id].GetText());
		}
		else if (m_items[id]->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[id]->Data);
			m_editor[id].ResetTextChanged();
			
			std::string edsrc = m_editor[id].GetText();

			shader->Owner->SaveCodeEditorItem(edsrc.c_str(), edsrc.size(), m_shaderTypeId[id]);
		}
	}
	void CodeEditorUI::m_compile(int id)
	{
		if (m_trackerRunning) {
			std::lock_guard<std::mutex> lock(m_trackFilesMutex);
			m_trackIgnore.push_back(m_items[id]->Name);
		}

		m_save(id);
		m_data->Renderer.Recompile(m_items[id]->Name);
	}
	void CodeEditorUI::m_loadEditorShortcuts(TextEditor* ed)
	{
		auto sMap = KeyboardShortcuts::Instance().GetMap();

		for (auto it = sMap.begin(); it != sMap.end(); it++)
		{
			std::string id = it->first;

			if (id.substr(0, 6) == "Editor") {
				std::string name = id.substr(7);

				TextEditor::ShortcutID sID = TextEditor::ShortcutID::Count;
				for (int i = 0; i < (int)TextEditor::ShortcutID::Count; i++) {
					if (EDITOR_SHORTCUT_NAMES[i] == name) {
						sID = (TextEditor::ShortcutID)i;
						break;
					}
				}

				if (sID != TextEditor::ShortcutID::Count)
					ed->SetShortcut(sID, TextEditor::Shortcut(it->second.Key1, it->second.Key2, it->second.Alt, it->second.Ctrl, it->second.Shift));
			}
		}
	}

	void CodeEditorUI::UpdateAutoRecompileItems() 
	{
		if (m_autoRecompileRequest) {
        	std::shared_lock<std::shared_mutex> lk(m_autoRecompilerMutex);
			for (const auto& it : m_ariiList) {
				if (it.second.SPass != nullptr)
					m_data->Renderer.RecompileFromSource(it.first.c_str(), it.second.VS, it.second.PS, it.second.GS);
				else if (it.second.CPass != nullptr)
					m_data->Renderer.RecompileFromSource(it.first.c_str(), it.second.CS);
				else if (it.second.APass != nullptr)
					m_data->Renderer.RecompileFromSource(it.first.c_str(), it.second.AS);
				else if (it.second.PluginData != nullptr)
					it.second.PluginData->Owner->HandleRecompileFromSource(it.first.c_str(), it.second.PluginID, it.second.PluginCode.c_str(), it.second.PluginCode.size());
			}
			if (m_autoRecompileCachedMsgs.size() > 0)
				m_data->Messages.Add(m_autoRecompileCachedMsgs);
			m_autoRecompileRequest = false;
		}
	}
	void CodeEditorUI::SetAutoRecompile(bool autorec)
	{
		if (m_autoRecompile == autorec)
			return;
		
		m_autoRecompile = autorec;

		if (autorec) {
			Logger::Get().Log("Starting auto-recompiler...");

			if (m_autoRecompileThread != nullptr) {
				m_autoRecompilerRunning = false;
				if (m_autoRecompileThread->joinable())
					m_autoRecompileThread->join();

				delete m_autoRecompileThread;
				m_autoRecompileThread = nullptr;
			}

			m_autoRecompilerRunning = true;

			m_autoRecompileThread = new std::thread(&CodeEditorUI::m_autoRecompiler, this);
		}
		else {
			Logger::Get().Log("Stopping auto-recompiler...");

			m_autoRecompilerRunning = false;

			if (m_autoRecompileThread->joinable())
				m_autoRecompileThread->join();
			delete m_autoRecompileThread;
			m_autoRecompileThread = nullptr;
		}
	}
	void CodeEditorUI::m_autoRecompiler()
	{		
		while (m_autoRecompilerRunning) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));

			// get all the sources
			if (!m_autoRecompilerMutex.try_lock())
				continue;

			m_ariiList.clear();
			for (int i = 0; i < m_editor.size(); i++) {
				if (!m_editor[i].IsTextChanged())
					continue;

				if (m_items[i]->Type == PipelineItem::ItemType::ShaderPass) {
					AutoRecompilerItemInfo* inf = &m_ariiList[m_items[i]->Name];
					pipe::ShaderPass* pass = (pipe::ShaderPass*)m_items[i]->Data;
					inf->SPass = pass;
					inf->CPass = nullptr;
					inf->APass = nullptr;
					inf->PluginData = nullptr;
					
					if (m_shaderTypeId[i] == 0) {
						inf->VS = m_editor[i].GetText();
						inf->VS_SLang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->VSPath);
					} else if (m_shaderTypeId[i] == 1) {
						inf->PS = m_editor[i].GetText();
						inf->PS_SLang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->PSPath);
					} else if (m_shaderTypeId[i] == 2) {
						inf->GS = m_editor[i].GetText();
						inf->GS_SLang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->GSPath);
					}
				}
				else if (m_items[i]->Type == PipelineItem::ItemType::ComputePass) {
					AutoRecompilerItemInfo* inf = &m_ariiList[m_items[i]->Name];
					pipe::ComputePass* pass = (pipe::ComputePass*)m_items[i]->Data;
					inf->CPass = pass;
					inf->SPass = nullptr;
					inf->APass = nullptr;
					inf->PluginData = nullptr;

					inf->CS = m_editor[i].GetText();
					inf->CS_SLang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->Path);
				}
				else if (m_items[i]->Type == PipelineItem::ItemType::AudioPass) {
					AutoRecompilerItemInfo *inf = &m_ariiList[m_items[i]->Name];
					pipe::AudioPass *pass = (pipe::AudioPass *)m_items[i]->Data;
					inf->APass = pass;
					inf->SPass = nullptr;
					inf->CPass = nullptr;
					inf->PluginData = nullptr;
					
					inf->AS = m_editor[i].GetText();
				}
				else if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
					AutoRecompilerItemInfo* inf = &m_ariiList[m_items[i]->Name];
					pipe::PluginItemData* pass = (pipe::PluginItemData*)m_items[i]->Data;
					inf->APass = nullptr;
					inf->SPass = nullptr;
					inf->CPass = nullptr;
					inf->PluginData = pass;

					inf->PluginCode = m_editor[i].GetText();
					inf->PluginID = m_shaderTypeId[i];
				}
			}

			// cache
			std::vector<ed::MessageStack::Message> msgPrev = m_data->Messages.GetMessages();
			m_autoRecompileCachedMsgs.clear();

			for (auto& it : m_ariiList) {
				if (it.second.SPass != nullptr) {
					if (it.second.VS_SLang != ShaderLanguage::GLSL)
						it.second.VS = ShaderTranscompiler::TranscompileSource(it.second.VS_SLang, m_data->Parser.GetProjectPath(it.second.SPass->VSPath), it.second.VS, 0, it.second.SPass->VSEntry, it.second.SPass->Macros, it.second.SPass->GSUsed, &m_data->Messages, &m_data->Parser);
					if (it.second.PS_SLang != ShaderLanguage::GLSL)
						it.second.PS = ShaderTranscompiler::TranscompileSource(it.second.PS_SLang, m_data->Parser.GetProjectPath(it.second.SPass->PSPath), it.second.PS, 1, it.second.SPass->PSEntry, it.second.SPass->Macros, it.second.SPass->GSUsed, &m_data->Messages, &m_data->Parser);
					if (it.second.GS_SLang != ShaderLanguage::GLSL)
						it.second.GS = ShaderTranscompiler::TranscompileSource(it.second.GS_SLang, m_data->Parser.GetProjectPath(it.second.SPass->GSPath), it.second.GS, 2, it.second.SPass->GSEntry, it.second.SPass->Macros, it.second.SPass->GSUsed, &m_data->Messages, &m_data->Parser);
				} else if (it.second.CPass != nullptr) {
					if (it.second.CS_SLang != ShaderLanguage::GLSL)
						it.second.CS = ShaderTranscompiler::TranscompileSource(it.second.CS_SLang, m_data->Parser.GetProjectPath(it.second.CPass->Path), it.second.CS, 3, it.second.CPass->Entry, it.second.CPass->Macros, false, &m_data->Messages, &m_data->Parser);
				}
			}

			std::vector<ed::MessageStack::Message> msgCurr = m_data->Messages.GetMessages();
			for (int i = 0; i < msgCurr.size(); i++) {
				for (int j = 0; j < msgPrev.size(); j++) {
					if (msgCurr[i].Text == msgPrev[j].Text) {
						msgCurr.erase(msgCurr.begin() + i);
						msgPrev.erase(msgPrev.begin() + j);
						i--;
						break;
					}
				}
			}
			m_autoRecompileCachedMsgs = msgCurr;

			m_autoRecompileRequest = m_ariiList.size()>0;

			m_autoRecompilerMutex.unlock();
		}
	}

	void CodeEditorUI::SetTrackFileChanges(bool track)
	{
		if (m_trackFileChanges == track)
			return;
		
		m_trackFileChanges = track;

		if (track) {
			Logger::Get().Log("Starting to track file changes...");

			if (m_trackThread != nullptr) {
				m_trackerRunning = false;
				if (m_trackThread->joinable())
					m_trackThread->join();

				delete m_trackThread;
				m_trackThread = nullptr;
			}

			m_trackerRunning = true;

			m_trackThread = new std::thread(&CodeEditorUI::m_trackWorker, this);
		}
		else {
			Logger::Get().Log("Stopping file change tracking...");

			m_trackerRunning = false;

			if (m_trackThread->joinable())
				m_trackThread->join();

			delete m_trackThread;
			m_trackThread = nullptr;
		}
	}
	void CodeEditorUI::m_trackWorker()
	{
		std::string curProject = m_data->Parser.GetOpenedFile();

		std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
		std::vector<bool> gsUsed(passes.size());

		std::vector<std::string> allFiles;		// list of all files we care for
		std::vector<std::string> allPasses;		// list of shader pass names that correspond to the file name
		std::vector<std::string> paths;			// list of all paths that we should have "notifications turned on"

		m_trackUpdatesNeeded = 0;

	#if defined(__APPLE__)
		// TODO: implementation for macos (cant test)
	#elif defined(__linux__) || defined(__unix__)
	
		int bufLength, bufIndex = 0;
		int notifyEngine = inotify_init1(IN_NONBLOCK);
		char buffer[EVENT_BUF_LEN];

		std::vector<int> notifyIDs;
		
		if (notifyEngine < 0) {
			// TODO: log from this thread!
			return;
		}

	#elif defined(_WIN32)
		// variables for storing all the handles
		std::vector<HANDLE> events(paths.size());
		std::vector<HANDLE> hDirs(paths.size());
		std::vector<OVERLAPPED> pOverlap(paths.size());

		// buffer data
		const int bufferLen = 2048;
		char buffer[bufferLen];
		DWORD bytesReturned;
		char filename[MAX_PATH];
	#endif

		// run this loop until we close the thread
		while (m_trackerRunning)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			for (int j = 0; j < m_trackedNeedsUpdate.size(); j++)
				m_trackedNeedsUpdate[j] = false;

			// check if user added/changed shader paths
			std::vector<PipelineItem*> nPasses = m_data->Pipeline.GetList();
			bool needsUpdate = false;
			for (auto& pass : nPasses) {
				if (pass->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* data = (pipe::ShaderPass*)pass->Data;

					bool foundVS = false, foundPS = false, foundGS = false;

					std::string vsPath(m_data->Parser.GetProjectPath(data->VSPath));
					std::string psPath(m_data->Parser.GetProjectPath(data->PSPath));

					for (auto& f : allFiles) {
						if (f == vsPath) {
							foundVS = true;
							if (foundPS) break;
						} else if (f == psPath) {
							foundPS = true;
							if (foundVS) break;
						}
					}

					if (data->GSUsed) {
						std::string gsPath(m_data->Parser.GetProjectPath(data->GSPath));
						for (auto& f : allFiles)
							if (f == gsPath) {
								foundGS = true;
								break;
							}
					}
					else foundGS = true;

					if (!foundGS || !foundVS || !foundPS) {
						needsUpdate = true;
						break;
					}
				}
				else if (pass->Type == PipelineItem::ItemType::ComputePass) {
					pipe::ComputePass *data = (pipe::ComputePass *)pass->Data;

					bool found = false;

					std::string path(m_data->Parser.GetProjectPath(data->Path));

					for (auto &f : allFiles)
						if (f == path)
							found = true;

					if (!found) {
						needsUpdate = true;
						break;
					}
				} 
				else if (pass->Type == PipelineItem::ItemType::AudioPass) {
					pipe::AudioPass *data = (pipe::AudioPass *)pass->Data;

					bool found = false;

					std::string path(m_data->Parser.GetProjectPath(data->Path));

					for (auto &f : allFiles)
						if (f == path)
							found = true;

					if (!found) {
						needsUpdate = true;
						break;
					}
				}
				else if (pass->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* data = (pipe::PluginItemData*)pass->Data;

					if (data->Owner->HasShaderFilePathChanged()) {
						needsUpdate = true;
						data->Owner->UpdateShaderFilePath();
					}
				}
			}

			for (int i = 0; i < gsUsed.size() && i < nPasses.size(); i++) {
				if (nPasses[i]->Type == PipelineItem::ItemType::ShaderPass) {
					bool used = ((pipe::ShaderPass*)nPasses[i]->Data)->GSUsed;
					if (gsUsed[i] != used) {
						gsUsed[i] = used;
						needsUpdate = true;
					}
				}
			}

			// update our file collection if needed
			if (needsUpdate || nPasses.size() != passes.size() || curProject != m_data->Parser.GetOpenedFile() || paths.size() == 0) {
		#if defined(__APPLE__)
				// TODO: implementation for macos
		#elif defined(__linux__) || defined(__unix__)
				for (int i = 0; i < notifyIDs.size(); i++)
					inotify_rm_watch(notifyEngine, notifyIDs[i]);
				notifyIDs.clear();
		#elif defined(_WIN32)
				for (int i = 0; i < paths.size(); i++) {
					CloseHandle(hDirs[i]);
					CloseHandle(events[i]);
				}
				events.clear();
				hDirs.clear();
		#endif

				allFiles.clear();
				allPasses.clear();
				paths.clear();
				curProject = m_data->Parser.GetOpenedFile();
				
				// get all paths to all shaders
				passes = nPasses;
				gsUsed.resize(passes.size());
				m_trackedNeedsUpdate.resize(passes.size());
				for (const auto& pass : passes) {
					if (pass->Type == PipelineItem::ItemType::ShaderPass) {
						pipe::ShaderPass* data = (pipe::ShaderPass*)pass->Data;

						std::string vsPath(m_data->Parser.GetProjectPath(data->VSPath));
						std::string psPath(m_data->Parser.GetProjectPath(data->PSPath));

						allFiles.push_back(vsPath);
						paths.push_back(vsPath.substr(0, vsPath.find_last_of("/\\") + 1));
						allPasses.push_back(pass->Name);

						allFiles.push_back(psPath);
						paths.push_back(psPath.substr(0, psPath.find_last_of("/\\") + 1));
						allPasses.push_back(pass->Name);

						if (data->GSUsed) {
							std::string gsPath(m_data->Parser.GetProjectPath(data->GSPath));

							allFiles.push_back(gsPath);
							paths.push_back(gsPath.substr(0, gsPath.find_last_of("/\\") + 1));
							allPasses.push_back(pass->Name);
						}
					} 
					else if (pass->Type == PipelineItem::ItemType::ComputePass) {
						pipe::ComputePass *data = (pipe::ComputePass*)pass->Data;

						std::string path(m_data->Parser.GetProjectPath(data->Path));

						allFiles.push_back(path);
						paths.push_back(path.substr(0, path.find_last_of("/\\") + 1));
						allPasses.push_back(pass->Name);
					} 
					else if (pass->Type == PipelineItem::ItemType::AudioPass) {
						pipe::AudioPass *data = (pipe::AudioPass*)pass->Data;

						std::string path(m_data->Parser.GetProjectPath(data->Path));

						allFiles.push_back(path);
						paths.push_back(path.substr(0, path.find_last_of("/\\") + 1));
						allPasses.push_back(pass->Name);
					}
					else if (pass->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* data = (pipe::PluginItemData*)pass->Data;


						int count = data->Owner->GetShaderFilePathCount();

						for (int i = 0; i < count; i++) {
							std::string path(m_data->Parser.GetProjectPath(data->Owner->GetShaderFilePath(i)));

							allFiles.push_back(path);
							paths.push_back(path.substr(0, path.find_last_of("/\\") + 1));
							allPasses.push_back(pass->Name);
						}
					}
				}

				// delete directories that appear twice or that are subdirectories
				{
					std::vector<bool> toDelete(paths.size(), false);

					for (int i = 0; i < paths.size(); i++) {
						if (toDelete[i]) continue;

						for (int j = 0; j < paths.size(); j++) {
							if (j == i || toDelete[j]) continue;

							if (paths[j].find(paths[i]) != std::string::npos)
								toDelete[j] = true;
						}
					}

					for (int i = 0; i < paths.size(); i++)
						if (toDelete[i]) {
							paths.erase(paths.begin() + i);
							toDelete.erase(toDelete.begin() + i);
							i--;
						}
				}

		#if defined(__APPLE__)
						// TODO: implementation for macos
		#elif defined(__linux__) || defined(__unix__)
				// create HANDLE to all tracked directories
				notifyIDs.resize(paths.size());
				for (int i = 0; i < paths.size(); i++)
					notifyIDs[i] = inotify_add_watch(notifyEngine, paths[i].c_str(), IN_MODIFY);
		#elif defined(_WIN32)
				events.resize(paths.size());
				hDirs.resize(paths.size());
				pOverlap.resize(paths.size());

				// create HANDLE to all tracked directories
				for (int i = 0; i < paths.size(); i++) {
					hDirs[i] = CreateFileA(paths[i].c_str(), GENERIC_READ | FILE_LIST_DIRECTORY,
						FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
						NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
						NULL);

					if (hDirs[i] == INVALID_HANDLE_VALUE)
						return;

					pOverlap[i].OffsetHigh = 0;
					pOverlap[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

					events[i] = pOverlap[i].hEvent;
				}
		#endif
			}

			if (paths.size() == 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
			}

		#if defined(__APPLE__)
					// TODO: implementation for macos
		#elif defined(__linux__) || defined(__unix__)
			fd_set rfds;
			int eCount = select(notifyEngine+1, &rfds, NULL, NULL, NULL);
			
			// check for changes
			bufLength = read(notifyEngine, buffer, EVENT_BUF_LEN ); 
			if (bufLength < 0) { /* TODO: error! */ }  

			// read all events
			while (bufIndex < bufLength) {
				struct inotify_event *event = (struct inotify_event*)&buffer[bufIndex];
				if (event->len) {
					if (event->mask & IN_MODIFY) {
						if (event->mask & IN_ISDIR) { /* it is a directory - do nothing */ }
						else {
							// check if its our shader and push it on the update queue if it is
							char filename[MAX_PATH];
							strcpy(filename, event->name);

							int pathIndex = 0;
							for (int i = 0; i < notifyIDs.size(); i++) {
								if (event->wd == notifyIDs[i]) {
									pathIndex = i;
									break;
								}
							}

							std::lock_guard<std::mutex> lock(m_trackFilesMutex);
							std::string updatedFile(paths[pathIndex] + filename);

							for (int i = 0; i < allFiles.size(); i++)
								if (allFiles[i] == updatedFile) {
									// did we modify this file through "Compile" option?
									bool shouldBeIgnored = false;
									for (int j = 0; j < m_trackIgnore.size(); j++)
										if (m_trackIgnore[j] == allPasses[i]) {
											shouldBeIgnored = true;
											m_trackIgnore.erase(m_trackIgnore.begin() + j);
											break;
										}

									if (!shouldBeIgnored) {
										m_trackUpdatesNeeded++;

										for (int j = 0; j < passes.size(); j++)
											if (allPasses[i] == passes[j]->Name)
												m_trackedNeedsUpdate[j] = true;
									}
								}
						}
					}
				}
				bufIndex += EVENT_SIZE + event->len;
			}
			bufIndex = 0;
		}

		for (int i = 0; i < notifyIDs.size(); i++)
			inotify_rm_watch(notifyEngine, notifyIDs[i]);
		close(notifyEngine);
	#elif defined(_WIN32)
			// notification data
			FILE_NOTIFY_INFORMATION* notif;
			int bufferOffset;

			// check for changes
			for (int i = 0; i < paths.size(); i++) {
				ReadDirectoryChangesW(
					hDirs[i],
					&buffer,
					bufferLen * sizeof(char),
					TRUE,
					FILE_NOTIFY_CHANGE_SIZE |
					FILE_NOTIFY_CHANGE_LAST_WRITE,
					&bytesReturned,
					&pOverlap[i],
					NULL);
			}

			DWORD dwWaitStatus = WaitForMultipleObjects(paths.size(), events.data(), false, 1000);

			// check if we got any info
			if (dwWaitStatus != WAIT_TIMEOUT) {
				bufferOffset = 0;
				do
				{
					// get notification data
					notif = (FILE_NOTIFY_INFORMATION*)((char*)buffer + bufferOffset);
					strcpy_s(filename, "");
					int filenamelen = WideCharToMultiByte(CP_ACP, 0, notif->FileName, notif->FileNameLength / 2, filename, sizeof(filename), NULL, NULL);
					if (filenamelen == 0)
						continue;
					filename[notif->FileNameLength / 2] = 0;

					if (notif->Action == FILE_ACTION_MODIFIED) {
						std::lock_guard<std::mutex> lock(m_trackFilesMutex);

						std::string updatedFile(paths[dwWaitStatus - WAIT_OBJECT_0] + std::string(filename));

						for (int i = 0; i < allFiles.size(); i++)
							if (allFiles[i] == updatedFile) {
								// did we modify this file through "Compile" option?
								bool shouldBeIgnored = false;
								for (int j = 0; j < m_trackIgnore.size(); j++)
									if (m_trackIgnore[j] == allPasses[i]) {
										shouldBeIgnored = true;
										m_trackIgnore.erase(m_trackIgnore.begin() + j);
										break;
									}

								if (!shouldBeIgnored) {
									m_trackUpdatesNeeded++;

									for (int j = 0; j < passes.size(); j++)
										if (allPasses[i] == passes[j]->Name)
											m_trackedNeedsUpdate[j] = true;
								}
							}
					}

					bufferOffset += notif->NextEntryOffset;
				} while (notif->NextEntryOffset);
			}
		}
		for (int i = 0; i < hDirs.size(); i++)
			CloseHandle(hDirs[i]);
		for (int i = 0; i < events.size(); i++)
			CloseHandle(events[i]);
		events.clear();
		hDirs.clear();
	#endif
	}

	TextEditor::LanguageDefinition CodeEditorUI::m_buildLanguageDefinition(IPlugin* plugin, int sid, const char* itemType, const char* filePath)
	{
		std::function<TextEditor::Identifier(const std::string&)> desc = [](const std::string& str) {
			TextEditor::Identifier id;
			id.mDeclaration = str;
			return id;
		};

		TextEditor::LanguageDefinition langDef;

		int keywordCount = plugin->GetLanguageDefinitionKeywordCount(sid);
		const char** keywords = plugin->GetLanguageDefinitionKeywords(sid);

		for (int i = 0; i < keywordCount; i++)
			langDef.mKeywords.insert(keywords[i]);

		int identifierCount = plugin->GetLanguageDefinitionIdentifierCount(sid);
		for (int i = 0; i < identifierCount; i++) {
			const char* ident = plugin->GetLanguageDefinitionIdentifier(i, sid);
			const char* identDesc = plugin->GetLanguageDefinitionIdentifierDesc(i, sid);
			langDef.mIdentifiers.insert(std::make_pair(ident, desc(identDesc)));
		}
		// m_GLSLDocumentation(langDef.mIdentifiers);

		int tokenRegexs = plugin->GetLanguageDefinitionTokenRegexCount(sid);
		for (int i = 0; i < tokenRegexs; i++) {
			plugin::TextEditorPaletteIndex palIndex;
			const char* regStr = plugin->GetLanguageDefinitionTokenRegex(i, palIndex, sid);
			langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>(regStr, (TextEditor::PaletteIndex)palIndex));
		}

		langDef.mCommentStart = plugin->GetLanguageDefinitionCommentStart(sid);
		langDef.mCommentEnd = plugin->GetLanguageDefinitionCommentEnd(sid);
		langDef.mSingleLineComment = plugin->GetLanguageDefinitionLineComment(sid);

		langDef.mCaseSensitive = plugin->IsLanguageDefinitionCaseSensitive(sid);
		langDef.mAutoIndentation = plugin->GetLanguageDefinitionAutoIndent(sid);

		langDef.mName = plugin->GetLanguageDefinitionName(sid);

		return langDef;
	}
	
	void CodeEditorUI::StatsPage::Fetch(ed::PipelineItem* item, const std::string& code, int typeId)
	{
	}
	void CodeEditorUI::StatsPage::Render()
	{
		// TODO: reimplement this
		ImGui::Text("This function doesn't work anymore after the switch to OpenGL.");
		ImGui::Text("Might be reimplemented in future.");
	}
}