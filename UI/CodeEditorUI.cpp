#include "CodeEditorUI.h"
#include "UIHelper.h"
#include "../Objects/Names.h"
#include "../Objects/Logger.h"
#include "../Objects/Settings.h"
#include "../Objects/ShaderTranscompiler.h"
#include "../Objects/ThemeContainer.h"
#include "../Objects/KeyboardShortcuts.h""

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <iostream>
#include <fstream>

#if defined(_WIN32)
	#include <windows.h>
#elif defined(__linux__) || defined(__unix__)
	#include <fcntl.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/inotify.h>
	#define EVENT_SIZE  ( sizeof (struct inotify_event) )
	#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
#elif defined(__APPLE__)
	#include <unistd.h>
	#include <sys/types.h>
#endif



#define STATUSBAR_HEIGHT 20 * Settings::Instance().DPIScale

namespace ed
{
	CodeEditorUI::CodeEditorUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name, bool visible)
		: UIView(ui, objects, name, visible), m_selectedItem(-1)
	{
		Settings& sets = Settings::Instance();

		if (ghc::filesystem::exists(sets.Editor.Font))
			m_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(sets.Editor.Font, sets.Editor.FontSize);
		else {
			m_font = ImGui::GetIO().Fonts->AddFontDefault();
			Logger::Get().Log("Failed to load editor font", true);
		}

		m_fontFilename = sets.Editor.Font;
		m_fontSize = sets.Editor.FontSize;
		m_fontNeedsUpdate = false;
		m_savePopupOpen = -1;
		m_focusStage = ShaderStage::Vertex;
		m_focusWindow = false;
		m_trackFileChanges = false;
		m_trackThread = nullptr;
		m_autoRecompileThread = nullptr;
		m_autoRecompilerRunning = false;
		m_autoRecompile = false;

		m_setupShortcuts();
	}
	CodeEditorUI::~CodeEditorUI() {
		delete m_autoRecompileThread;
		delete m_trackThread;
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
	void CodeEditorUI::m_save(int id)
	{
		bool canSave = true;

		// prompt user to choose a project location first
		if (m_data->Parser.GetOpenedFile() == "")
			canSave = m_ui->SaveAsProject(true);
		if (!canSave)
			return;

		if (m_items[id]->Type == PipelineItem::ItemType::ShaderPass) {
			ed::pipe::ShaderPass* shader = reinterpret_cast<ed::pipe::ShaderPass*>(m_items[id]->Data);

			m_editor[id].ResetTextChanged();

			if (m_shaderStage[id] == ShaderStage::Vertex)
				m_data->Parser.SaveProjectFile(shader->VSPath, m_editor[id].GetText());
			else if (m_shaderStage[id] == ShaderStage::Pixel)
				m_data->Parser.SaveProjectFile(shader->PSPath, m_editor[id].GetText());
			else if (m_shaderStage[id] == ShaderStage::Geometry)
				m_data->Parser.SaveProjectFile(shader->GSPath, m_editor[id].GetText());
		}
		else if (m_items[id]->Type == PipelineItem::ItemType::ComputePass) {
			ed::pipe::ComputePass* shader = reinterpret_cast<ed::pipe::ComputePass*>(m_items[id]->Data);
			m_editor[id].ResetTextChanged();
			m_data->Parser.SaveProjectFile(shader->Path, m_editor[id].GetText());
		}
		else if (m_items[id]->Type == PipelineItem::ItemType::AudioPass) {
			ed::pipe::AudioPass* shader = reinterpret_cast<ed::pipe::AudioPass*>(m_items[id]->Data);
			m_editor[id].ResetTextChanged();
			m_data->Parser.SaveProjectFile(shader->Path, m_editor[id].GetText());
		}
		else if (m_items[id]->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[id]->Data);
			m_editor[id].ResetTextChanged();

			std::string edsrc = m_editor[id].GetText();

			shader->Owner->SaveCodeEditorItem(edsrc.c_str(), edsrc.size(), (int)m_shaderStage[id]); // TODO: custom stages
		}
	}
	void CodeEditorUI::m_compile(int id)
	{
		if (m_trackerRunning) {
			std::lock_guard<std::mutex> lock(m_trackFilesMutex);
			m_trackIgnore.push_back(m_items[id]->Name);
		}

		m_save(id);

		std::string shaderFile = "";
		if (m_items[id]->Type == PipelineItem::ItemType::ShaderPass) {
			ed::pipe::ShaderPass* shader = reinterpret_cast<ed::pipe::ShaderPass*>(m_items[id]->Data);
			if (m_shaderStage[id] == ShaderStage::Vertex)
				shaderFile = shader->VSPath;
			else if (m_shaderStage[id] == ShaderStage::Pixel)
				shaderFile = shader->PSPath;
			else if (m_shaderStage[id] == ShaderStage::Geometry)
				shaderFile = shader->GSPath;
		}
		else if (m_items[id]->Type == PipelineItem::ItemType::ComputePass) {
			ed::pipe::ComputePass* shader = reinterpret_cast<ed::pipe::ComputePass*>(m_items[id]->Data);
			shaderFile = shader->Path;
		}
		else if (m_items[id]->Type == PipelineItem::ItemType::AudioPass) {
			ed::pipe::AudioPass* shader = reinterpret_cast<ed::pipe::AudioPass*>(m_items[id]->Data);
			shaderFile = shader->Path;
		}
		else if (m_items[id]->Type == PipelineItem::ItemType::PluginItem) {
			ed::pipe::PluginItemData* shader = reinterpret_cast<ed::pipe::PluginItemData*>(m_items[id]->Data);
			shader->Owner->HandleRecompile(m_items[id]->Name);
		}

		m_data->Renderer.RecompileFile(shaderFile.c_str());
	}

	void CodeEditorUI::OnEvent(const SDL_Event& e) { }
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

				std::string shaderType = isPluginItem ? plData->Owner->GetLanguageAbbreviation((int)m_shaderStage[i]) : m_shaderStage[i] == ShaderStage::Vertex ? "VS" : (m_shaderStage[i] == ShaderStage::Pixel ? "PS" : (m_shaderStage[i] == ShaderStage::Geometry ? "GS" : "CS"));
				std::string windowName(std::string(m_items[i]->Name) + " (" + shaderType + ")");
				
				if (m_editor[i].IsTextChanged() && !m_data->Parser.IsProjectModified())
					m_data->Parser.ModifyProject();

				ImGui::SetNextWindowSizeConstraints(ImVec2(300, 300), ImVec2(300, 300));
				ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
				if (ImGui::Begin((std::string(windowName) + "###code_view" + shaderType + std::to_string(wid[isPluginItem ? 4 : (int)m_shaderStage[i]])).c_str(), &m_editorOpen[i], (ImGuiWindowFlags_UnsavedDocument * m_editor[i].IsTextChanged()) | ImGuiWindowFlags_MenuBar)) {
					if (ImGui::BeginMenuBar()) {
						if (ImGui::BeginMenu("File")) {
							if (ImGui::MenuItem("Save", KeyboardShortcuts::Instance().GetString("CodeUI.Save").c_str())) m_save(i);
							ImGui::EndMenu();
						}
						if (ImGui::BeginMenu("Code")) {
							if (ImGui::MenuItem("Compile", KeyboardShortcuts::Instance().GetString("CodeUI.Compile").c_str())) m_compile(i);

							if (!m_stats[i].IsActive && ImGui::MenuItem("Stats", KeyboardShortcuts::Instance().GetString("CodeUI.SwitchView").c_str(), nullptr, false)) m_stats[i].Fetch(m_items[i], m_editor[i].GetText(), (int)m_shaderStage[i]);
							
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
							if (msgs[j].Line > 0 && msgs[j].Group == m_items[i]->Name && msgs[j].Shader == (int)m_shaderStage[i])
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
					if (m_focusItem == m_items[i]->Name && m_focusStage == m_shaderStage[i]) {
						ImGui::SetWindowFocus();
						m_focusWindow = false;
					}
				}
		
				wid[isPluginItem ? 4 : (int)m_shaderStage[i]]++;
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
						shader->Owner->CloseCodeEditorItem((int)m_shaderStage[i]);
					}

					m_items.erase(m_items.begin() + i);
					m_editor.erase(m_editor.begin() + i);
					m_editorOpen.erase(m_editorOpen.begin() + i);
					m_stats.erase(m_stats.begin() + i);
					m_paths.erase(m_paths.begin() + i);
					m_shaderStage.erase(m_shaderStage.begin() + i);
					i--;
				}
			}
		}
	}

	void CodeEditorUI::SetTheme(const TextEditor::Palette& colors) {
		for (TextEditor& editor : m_editor)
			editor.SetPalette(colors);
	}
	void CodeEditorUI::SetTabSize(int ts) {
		for (TextEditor& editor : m_editor)
			editor.SetTabSize(ts);
	}
	void CodeEditorUI::SetInsertSpaces(bool ts) {
		for (TextEditor& editor : m_editor)
			editor.SetInsertSpaces(ts);
	}
	void CodeEditorUI::SetSmartIndent(bool ts) {
		for (TextEditor& editor : m_editor)
			editor.SetSmartIndent(ts);
	}
	void CodeEditorUI::SetShowWhitespace(bool wh) {
		for (TextEditor& editor : m_editor)
			editor.SetShowWhitespaces(wh);
	}
	void CodeEditorUI::SetHighlightLine(bool ts) {
		for (TextEditor& editor : m_editor)
			editor.SetHighlightLine(ts);
	}
	void CodeEditorUI::SetShowLineNumbers(bool ts) {
		for (TextEditor& editor : m_editor)
			editor.SetShowLineNumbers(ts);
	}
	void CodeEditorUI::SetCompleteBraces(bool ts) {
		for (TextEditor& editor : m_editor)
			editor.SetCompleteBraces(ts);
	}
	void CodeEditorUI::SetHorizontalScrollbar(bool ts) {
		for (TextEditor& editor : m_editor)
			editor.SetHorizontalScroll(ts);
	}
	void CodeEditorUI::SetSmartPredictions(bool ts) {
		for (TextEditor& editor : m_editor)
			editor.SetSmartPredictions(ts);
	}
	void CodeEditorUI::SetFunctionTooltips(bool ts) {
		for (TextEditor& editor : m_editor)
			editor.SetFunctionTooltips(ts);
	}
	void CodeEditorUI::UpdateShortcuts() {
		for (auto& editor : m_editor)
			m_loadEditorShortcuts(&editor);
	}
	void CodeEditorUI::SetFont(const std::string& filename, int size)
	{
		m_fontNeedsUpdate = m_fontFilename != filename || m_fontSize != size;
		m_fontFilename = filename;
		m_fontSize = size;
	}

	void CodeEditorUI::Open(PipelineItem* item, ShaderStage stage)
	{
		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			ed::pipe::ShaderPass* shader = reinterpret_cast<ed::pipe::ShaderPass*>(item->Data);

			std::string shaderPath = "";
			if (stage == ShaderStage::Vertex)
				shaderPath = m_data->Parser.GetProjectPath(shader->VSPath);
			else if (stage == ShaderStage::Pixel)
				shaderPath = m_data->Parser.GetProjectPath(shader->PSPath);
			else if (stage == ShaderStage::Geometry)
				shaderPath = m_data->Parser.GetProjectPath(shader->GSPath);

			if (Settings::Instance().General.UseExternalEditor) {
				UIHelper::ShellOpen(shaderPath);
				return;
			}

			// check if already opened
			for (int i = 0; i < m_paths.size(); i++) {
				if (m_paths[i] == shaderPath) {
					m_focusWindow = true;
					m_focusStage = stage;
					m_focusItem = m_items[i]->Name;
					return;
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
			editor->SetFunctionTooltips(Settings::Instance().Editor.FunctionTooltips);
			editor->SetPath(shaderPath);
			m_loadEditorShortcuts(editor);
			
			bool isHLSL = ShaderTranscompiler::GetShaderTypeFromExtension(shaderPath) == ShaderLanguage::HLSL;
			editor->SetLanguageDefinition(isHLSL ? defHLSL : defGLSL);
			
			m_shaderStage.push_back(stage);

			// apply breakpoints
			m_applyBreakpoints(editor, shaderPath);

			std::string shaderContent = m_data->Parser.LoadFile(shaderPath);
			m_paths.push_back(shaderPath);
			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		}
		else if (item->Type == PipelineItem::ItemType::ComputePass)
		{
			ed::pipe::ComputePass *shader = reinterpret_cast<ed::pipe::ComputePass *>(item->Data);
			std::string shaderPath = m_data->Parser.GetProjectPath(shader->Path);

			if (Settings::Instance().General.UseExternalEditor)
			{
				UIHelper::ShellOpen(shaderPath);
				return;
			}

			// check if already opened
			for (int i = 0; i < m_paths.size(); i++) {
				if (m_paths[i] == shaderPath) {
					m_focusWindow = true;
					m_focusStage = stage;
					m_focusItem = m_items[i]->Name;
					return;
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
			editor->SetFunctionTooltips(Settings::Instance().Editor.FunctionTooltips);
			editor->SetPath(shaderPath);
			m_loadEditorShortcuts(editor);

			// apply breakpoints
			m_applyBreakpoints(editor, shaderPath);

			bool isHLSL = ShaderTranscompiler::GetShaderTypeFromExtension(shader->Path) == ShaderLanguage::HLSL;
			editor->SetLanguageDefinition(isHLSL ? defHLSL : defGLSL);

			m_shaderStage.push_back(stage);

			std::string shaderContent = m_data->Parser.LoadFile(shaderPath);
			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		}
		else if (item->Type == PipelineItem::ItemType::AudioPass)
		{
			ed::pipe::AudioPass *shader = reinterpret_cast<ed::pipe::AudioPass *>(item->Data);
			std::string shaderPath = m_data->Parser.GetProjectPath(shader->Path);

			if (Settings::Instance().General.UseExternalEditor)
			{
				UIHelper::ShellOpen(shaderPath);
				return;
			}

			// check if already opened
			for (int i = 0; i < m_paths.size(); i++) {
				if (m_paths[i] == shaderPath) {
					m_focusWindow = true;
					m_focusStage = stage;
					m_focusItem = m_items[i]->Name;
					return;
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
			editor->SetFunctionTooltips(Settings::Instance().Editor.FunctionTooltips);
			editor->SetPath(shaderPath);
			m_loadEditorShortcuts(editor);

			// apply breakpoints & breakpoint functions
			m_applyBreakpoints(editor, shaderPath);

			bool isHLSL = ShaderTranscompiler::GetShaderTypeFromExtension(shader->Path) == ShaderLanguage::HLSL;
			editor->SetLanguageDefinition(isHLSL ? defHLSL : defGLSL);

			m_shaderStage.push_back(stage);

			std::string shaderContent = m_data->Parser.LoadFile(shaderPath);
			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		}
	}
	void CodeEditorUI::OpenPluginCode(PipelineItem* item, const char* filepath, int id)
	{
		if (item->Type == PipelineItem::ItemType::PluginItem)
		{
			pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(item->Data);

			if (Settings::Instance().General.UseExternalEditor)
			{
				std::string path = m_data->Parser.GetProjectPath(filepath);
				UIHelper::ShellOpen(path);
				return;
			}

			// check if already opened
			for (int i = 0; i < m_paths.size(); i++)
			{
				if (m_paths[i] == filepath)
				{
					m_focusWindow = true;
					m_focusStage = (ShaderStage)id; // TODO: this might not work anymore
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

			TextEditor::LanguageDefinition defPlugin = m_buildLanguageDefinition(shader->Owner, m_focusStage, shader->Type, filepath);

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
			editor->SetFunctionTooltips(Settings::Instance().Editor.FunctionTooltips);
			m_loadEditorShortcuts(editor);
			
			editor->SetLanguageDefinition(defPlugin);

			m_shaderStage.push_back(m_focusStage);

			std::string shaderContent = m_data->Parser.LoadProjectFile(filepath);
			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		}
	}
	TextEditor* CodeEditorUI::Get(PipelineItem* item, ShaderStage stage)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == item && m_shaderStage[i] == stage)
				return &m_editor[i];
		return nullptr;
	}

	void CodeEditorUI::CloseAll()
	{
		// delete not needed editors
		for (int i = 0; i < m_editorOpen.size(); i++) {
			if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[i]->Data);
				shader->Owner->CloseCodeEditorItem((int)m_shaderStage[i]); // TODO: this casting....
			}

			m_items.erase(m_items.begin() + i);
			m_editor.erase(m_editor.begin() + i);
			m_editorOpen.erase(m_editorOpen.begin() + i);
			m_stats.erase(m_stats.begin() + i);
			m_paths.erase(m_paths.begin() + i);
			m_shaderStage.erase(m_shaderStage.begin() + i);
			i--;
		}
	}
	void CodeEditorUI::CloseAllFrom(PipelineItem* item)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (m_items[i] == item) {
				if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[i]->Data);
					shader->Owner->CloseCodeEditorItem((int)m_shaderStage[i]);
				}

				m_items.erase(m_items.begin() + i);
				m_editor.erase(m_editor.begin() + i);
				m_editorOpen.erase(m_editorOpen.begin() + i);
				m_stats.erase(m_stats.begin() + i);
				m_paths.erase(m_paths.begin() + i);
				m_shaderStage.erase(m_shaderStage.begin() + i);
				i--;
			}
		}
	}
	void CodeEditorUI::SaveAll()
	{
		for (int i = 0; i < m_items.size(); i++)
			m_save(i);
	}
	std::vector<std::pair<std::string, ShaderStage>> CodeEditorUI::GetOpenedFiles()
	{
		std::vector<std::pair<std::string, ShaderStage>> ret;
		for (int i = 0; i < m_items.size(); i++)
			ret.push_back(std::make_pair(std::string(m_items[i]->Name), m_shaderStage[i]));
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
	void CodeEditorUI::m_applyBreakpoints(TextEditor* editor, const std::string& path)
	{
		const std::vector<sd::Breakpoint>& bkpts = m_data->Debugger.GetBreakpointList(path);
		const std::vector<bool>& states = m_data->Debugger.GetBreakpointStateList(path);

		for (size_t i = 0; i < bkpts.size(); i++)
			editor->AddBreakpoint(bkpts[i].Line, bkpts[i].IsConditional ? bkpts[i].Condition : "", states[i]);

		editor->OnBreakpointRemove = [&](TextEditor* ed, int line) {
			m_data->Debugger.Engine.ClearBreakpoint(line);
			m_data->Debugger.RemoveBreakpoint(ed->GetPath(), line);
		};
		editor->OnBreakpointUpdate = [&](TextEditor* ed, int line, const std::string& cond, bool enabled) {
			// save it for later use
			m_data->Debugger.AddBreakpoint(ed->GetPath(), line, cond, enabled);

			if (!enabled) return;

			if (cond.empty()) m_data->Debugger.Engine.AddBreakpoint(line);
			else m_data->Debugger.Engine.AddConditionalBreakpoint(line, cond);
		};
	}
	
	void CodeEditorUI::StopDebugging()
	{
		for (auto& ed : m_editor)
			ed.SetCurrentLineIndicator(-1);
	}
	void CodeEditorUI::StopThreads()
	{
		m_autoRecompilerRunning = false;
		if (m_autoRecompileThread)
			m_autoRecompileThread->detach();
		m_trackerRunning = false;
		if (m_trackThread)
			m_trackThread->detach();
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

			// stop if it was running before
			m_autoRecompilerRunning = false;
			if (m_autoRecompileThread != nullptr && m_autoRecompileThread->joinable())
				m_autoRecompileThread->join();
			delete m_autoRecompileThread;
			m_autoRecompileThread = nullptr;

			// rerun
			m_autoRecompilerRunning = true;
			m_autoRecompileThread = new std::thread(&CodeEditorUI::m_autoRecompiler, this);
		}
		else {
			Logger::Get().Log("Stopping auto-recompiler...");

			m_autoRecompilerRunning = false;

			if (m_autoRecompileThread && m_autoRecompileThread->joinable())
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
					
					if (m_shaderStage[i] == ShaderStage::Vertex) {
						inf->VS = m_editor[i].GetText();
						inf->VS_SLang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->VSPath);
					} else if (m_shaderStage[i] == ShaderStage::Pixel) {
						inf->PS = m_editor[i].GetText();
						inf->PS_SLang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->PSPath);
					} else if (m_shaderStage[i] == ShaderStage::Geometry) {
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
					inf->PluginID = (int)m_shaderStage[i]; // TODO
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

			// stop first (if running)
			m_trackerRunning = false;
			if (m_trackThread != nullptr && m_trackThread->joinable())
				m_trackThread->join();
			delete m_trackThread;
			m_trackThread = nullptr;

			// start
			m_trackerRunning = true;
			m_trackThread = new std::thread(&CodeEditorUI::m_trackWorker, this);
		}
		else {
			Logger::Get().Log("Stopping file change tracking...");

			m_trackerRunning = false;

			if (m_trackThread && m_trackThread->joinable())
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
			// TODO: log!
			return;
		}

		// make sure that read() doesn't block on exit
		int flags = fcntl(notifyEngine, F_GETFL, 0);
		flags = (flags | O_NONBLOCK);
		fcntl(notifyEngine, F_SETFL, flags);

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
		while (m_trackerRunning) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			for (int j = 0; j < m_trackedNeedsUpdate.size(); j++)
				m_trackedNeedsUpdate[j] = false;

			// check if user added/changed shader paths
			std::vector<PipelineItem*> nPasses = m_data->Pipeline.GetList();
			bool needsUpdate = false;
			for (auto& pass : nPasses) {
				if (pass->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* data = (pipe::ShaderPass*) pass->Data;

					bool foundVS = false, foundPS = false, foundGS = false;

					std::string vsPath(m_data->Parser.GetProjectPath(data->VSPath));
					std::string psPath(m_data->Parser.GetProjectPath(data->PSPath));

					for (auto& f : allFiles) {
						if (f == vsPath) {
							foundVS = true;
							if (foundPS) break;
						}
						else if (f == psPath) {
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
					pipe::ComputePass* data = (pipe::ComputePass*) pass->Data;

					bool found = false;

					std::string path(m_data->Parser.GetProjectPath(data->Path));

					for (auto& f : allFiles)
						if (f == path)
							found = true;

					if (!found) {
						needsUpdate = true;
						break;
					}
				}
				else if (pass->Type == PipelineItem::ItemType::AudioPass) {
					pipe::AudioPass* data = (pipe::AudioPass*) pass->Data;

					bool found = false;

					std::string path(m_data->Parser.GetProjectPath(data->Path));

					for (auto& f : allFiles)
						if (f == path)
							found = true;

					if (!found) {
						needsUpdate = true;
						break;
					}
				}
				else if (pass->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* data = (pipe::PluginItemData*) pass->Data;

					if (data->Owner->HasShaderFilePathChanged()) {
						needsUpdate = true;
						data->Owner->UpdateShaderFilePath();
					}
				}
			}

			for (int i = 0; i < gsUsed.size() && i < nPasses.size(); i++) {
				if (nPasses[i]->Type == PipelineItem::ItemType::ShaderPass) {
					bool used = ((pipe::ShaderPass*) nPasses[i]->Data)->GSUsed;
					if (gsUsed[i] != used) {
						gsUsed[i] = used;
						needsUpdate = true;
					}
				}
			}

			// update our file collection if needed
			if (needsUpdate || nPasses.size() != passes.size() || curProject != m_data->Parser.GetOpenedFile() ||
				paths.size() == 0) {
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
						pipe::ShaderPass* data = (pipe::ShaderPass*) pass->Data;

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
						pipe::ComputePass* data = (pipe::ComputePass*) pass->Data;

						std::string path(m_data->Parser.GetProjectPath(data->Path));

						allFiles.push_back(path);
						paths.push_back(path.substr(0, path.find_last_of("/\\") + 1));
						allPasses.push_back(pass->Name);
					}
					else if (pass->Type == PipelineItem::ItemType::AudioPass) {
						pipe::AudioPass* data = (pipe::AudioPass*) pass->Data;

						std::string path(m_data->Parser.GetProjectPath(data->Path));

						allFiles.push_back(path);
						paths.push_back(path.substr(0, path.find_last_of("/\\") + 1));
						allPasses.push_back(pass->Name);
					}
					else if (pass->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* data = (pipe::PluginItemData*) pass->Data;


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
		}
#elif defined(__linux__) || defined(__unix__)
			fd_set rfds;
			int eCount = select(notifyEngine + 1, &rfds, NULL, NULL, NULL);

			if (eCount <= 0) continue;

			// check for changes
			bufLength = read(notifyEngine, buffer, EVENT_BUF_LEN);
			if (bufLength < 0) { /* TODO: error! */ }

			// read all events
			while (bufIndex < bufLength) {
				struct inotify_event* event = (struct inotify_event*) & buffer[bufIndex];
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

	TextEditor::LanguageDefinition CodeEditorUI::m_buildLanguageDefinition(IPlugin* plugin, ShaderStage stage, const char* itemType, const char* filePath)
	{
		std::function<TextEditor::Identifier(const std::string&)> desc = [](const std::string& str) {
			TextEditor::Identifier id;
			id.mDeclaration = str;
			return id;
		};

		int sid = (int)stage; // TODO: custom plugin stages!!!

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
		ImGui::Text("This function currently isn't implemented.");
		ImGui::Text("TODO");
	}
}