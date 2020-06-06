#include <SHADERed/Objects/KeyboardShortcuts.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/UIHelper.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <filesystem>
#include <fstream>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__unix__)
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
#elif defined(__APPLE__)
#include <sys/types.h>
#include <unistd.h>
#endif

#define STATUSBAR_HEIGHT Settings::Instance().CalculateSize(30)

namespace ed {
	CodeEditorUI::CodeEditorUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name, bool visible)
			: UIView(ui, objects, name, visible)
			, m_selectedItem(-1)
	{
		Settings& sets = Settings::Instance();

		if (std::filesystem::exists(sets.Editor.Font))
			m_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(sets.Editor.Font, sets.Editor.FontSize);
		else {
			m_font = ImGui::GetIO().Fonts->AddFontDefault();
			Logger::Get().Log("Failed to load code editor font", true);
		}

		m_fontFilename = sets.Editor.Font;
		m_fontSize = sets.Editor.FontSize;
		m_fontNeedsUpdate = false;
		m_savePopupOpen = -1;
		m_focusStage = ShaderStage::Vertex;
		m_focusWindow = false;
		m_trackFileChanges = false;
		m_trackThread = nullptr;
		m_contentChanged = false;

		RequestedProjectSave = false;

		m_setupShortcuts();
	}
	CodeEditorUI::~CodeEditorUI()
	{
		delete m_trackThread;
	}

	void CodeEditorUI::m_setupShortcuts()
	{
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
			if (m_selectedItem == -1 || m_selectedItem >= m_items.size())
				return;

			m_stats[m_selectedItem].Visible = !m_stats[m_selectedItem].Visible;

			if (m_stats[m_selectedItem].Visible)
				m_stats[m_selectedItem].Refresh(m_items[m_selectedItem], m_shaderStage[m_selectedItem]);
		});
		KeyboardShortcuts::Instance().SetCallback("CodeUI.ToggleStatusbar", [=]() {
			Settings::Instance().Editor.StatusBar = !Settings::Instance().Editor.StatusBar;
		});
	}
	void CodeEditorUI::m_save(int id)
	{
		if (id >= m_editor.size())
			return;

		bool canSave = true;

		// prompt user to choose a project location first
		if (m_data->Parser.GetOpenedFile() == "") {
			canSave = m_ui->SaveAsProject(true);
			RequestedProjectSave = true;
		}
		if (!canSave)
			return;

		TextEditor* ed = m_editor[id];
		std::string text = ed->GetText();
		std::string path = "";

		if (m_items[id]->Type == PipelineItem::ItemType::ShaderPass) {
			ed::pipe::ShaderPass* shader = reinterpret_cast<ed::pipe::ShaderPass*>(m_items[id]->Data);

			if (m_shaderStage[id] == ShaderStage::Vertex)
				path = shader->VSPath;
			else if (m_shaderStage[id] == ShaderStage::Pixel)
				path = shader->PSPath;
			else if (m_shaderStage[id] == ShaderStage::Geometry)
				path = shader->GSPath;
		} else if (m_items[id]->Type == PipelineItem::ItemType::ComputePass) {
			ed::pipe::ComputePass* shader = reinterpret_cast<ed::pipe::ComputePass*>(m_items[id]->Data);
			path = shader->Path;
		} else if (m_items[id]->Type == PipelineItem::ItemType::AudioPass) {
			ed::pipe::AudioPass* shader = reinterpret_cast<ed::pipe::AudioPass*>(m_items[id]->Data);
			path = shader->Path;
		}

		ed->ResetTextChanged();

		if (m_items[id]->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[id]->Data);
			std::string edsrc = m_editor[id]->GetText();
			shader->Owner->SaveCodeEditorItem(edsrc.c_str(), edsrc.size(), (int)m_shaderStage[id]); // TODO: custom stages
		} else
			m_data->Parser.SaveProjectFile(path, text);
	}
	void CodeEditorUI::m_compile(int id)
	{
		if (id >= m_editor.size())
			return;

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
		} else if (m_items[id]->Type == PipelineItem::ItemType::ComputePass) {
			ed::pipe::ComputePass* shader = reinterpret_cast<ed::pipe::ComputePass*>(m_items[id]->Data);
			shaderFile = shader->Path;
		} else if (m_items[id]->Type == PipelineItem::ItemType::AudioPass) {
			ed::pipe::AudioPass* shader = reinterpret_cast<ed::pipe::AudioPass*>(m_items[id]->Data);
			shaderFile = shader->Path;
		} else if (m_items[id]->Type == PipelineItem::ItemType::PluginItem) {
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

		// editor windows
		for (int i = 0; i < m_editor.size(); i++) {
			if (m_editorOpen[i]) {
				bool isPluginItem = m_items[i]->Type == PipelineItem::ItemType::PluginItem;
				pipe::PluginItemData* plData = (pipe::PluginItemData*)m_items[i]->Data;

				std::string shaderType = isPluginItem ? plData->Owner->GetLanguageAbbreviation((int)m_shaderStage[i]) : m_shaderStage[i] == ShaderStage::Vertex ? "VS" : (m_shaderStage[i] == ShaderStage::Pixel ? "PS" : (m_shaderStage[i] == ShaderStage::Geometry ? "GS" : "CS"));
				std::string windowName(std::string(m_items[i]->Name) + " (" + shaderType + ")");

				if (m_editor[i]->IsTextChanged() && !m_data->Parser.IsProjectModified())
					m_data->Parser.ModifyProject();

				ImGui::SetNextWindowSizeConstraints(ImVec2(300, 300), ImVec2(300, 300));
				ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
				if (ImGui::Begin((std::string(windowName) + "###code_view" + shaderType + std::to_string(wid[isPluginItem ? 4 : (int)m_shaderStage[i]])).c_str(), &m_editorOpen[i], (ImGuiWindowFlags_UnsavedDocument * m_editor[i]->IsTextChanged()) | ImGuiWindowFlags_MenuBar)) {
					if (ImGui::BeginMenuBar()) {
						if (ImGui::BeginMenu("File")) {
							if (ImGui::MenuItem("Save", KeyboardShortcuts::Instance().GetString("CodeUI.Save").c_str())) m_save(i);
							ImGui::EndMenu();
						}
						if (ImGui::BeginMenu("Code")) {
							if (ImGui::MenuItem("Compile", KeyboardShortcuts::Instance().GetString("CodeUI.Compile").c_str())) m_compile(i);

							if (!m_stats[i].Visible && ImGui::MenuItem("Stats", KeyboardShortcuts::Instance().GetString("CodeUI.SwitchView").c_str())) {
								m_stats[i].Visible = true;
								m_stats[i].Refresh(m_items[i], m_shaderStage[i]);
							}
							
							if (m_stats[i].Visible && ImGui::MenuItem("Code", KeyboardShortcuts::Instance().GetString("CodeUI.SwitchView").c_str())) m_stats[i].Visible = false;
							ImGui::Separator();
							if (ImGui::MenuItem("Undo", "CTRL+Z", nullptr, m_editor[i]->CanUndo())) m_editor[i]->Undo();
							if (ImGui::MenuItem("Redo", "CTRL+Y", nullptr, m_editor[i]->CanRedo())) m_editor[i]->Redo();
							ImGui::Separator();
							if (ImGui::MenuItem("Cut", "CTRL+X")) m_editor[i]->Cut();
							if (ImGui::MenuItem("Copy", "CTRL+C")) m_editor[i]->Copy();
							if (ImGui::MenuItem("Paste", "CTRL+V")) m_editor[i]->Paste();
							if (ImGui::MenuItem("Select All", "CTRL+A")) m_editor[i]->SelectAll();

							ImGui::EndMenu();
						}

						ImGui::EndMenuBar();
					}

					if (m_stats[i].Visible) {
						ImGui::PushFont(m_font);
						m_stats[i].Update(delta);
						ImGui::PopFont();
					} else {
						// add error markers if needed
						auto msgs = m_data->Messages.GetMessages();
						int groupMsg = 0;
						TextEditor::ErrorMarkers groupErrs;
						for (int j = 0; j < msgs.size(); j++) {
							ed::MessageStack::Message* msg = &msgs[j];

							if (groupErrs.count(msg->Line))
								continue;

							if (msg->Line > 0 && msg->Group == m_items[i]->Name && msg->Shader == m_shaderStage[i])
								groupErrs[msg->Line] = msg->Text;
						}
						m_editor[i]->SetErrorMarkers(groupErrs);

						bool statusbar = Settings::Instance().Editor.StatusBar;

						// render code
						ImGui::PushFont(m_font);
						m_editor[i]->Render(windowName.c_str(), ImVec2(0, -statusbar * STATUSBAR_HEIGHT));
						if (ImGui::IsItemHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f) {
							Settings::Instance().Editor.FontSize = Settings::Instance().Editor.FontSize + ImGui::GetIO().MouseWheel;
							this->SetFont(Settings::Instance().Editor.Font, Settings::Instance().Editor.FontSize);
						}
						ImGui::PopFont();

						// status bar
						if (statusbar) {
							auto cursor = m_editor[i]->GetCursorPosition();

							ImGui::Separator();
							ImGui::Text("Line %d\tCol %d\tType: %s\tPath: %s", cursor.mLine, cursor.mColumn, m_editor[i]->GetLanguageDefinition().mName.c_str(), m_paths[i].c_str());
						}
					}

					if (m_editor[i]->IsFocused())
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
			if (!m_editorOpen[i] && m_editor[i]->IsTextChanged()) {
				ImGui::OpenPopup("Save Changes##code_save");
				m_savePopupOpen = i;
				m_editorOpen[i] = true;
				break;
			}

		// Create Item popup
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

		// delete closed editors
		if (m_savePopupOpen == -1) {
			for (int i = 0; i < m_editorOpen.size(); i++) {
				if (!m_editorOpen[i]) {
					if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[i]->Data);
						shader->Owner->CloseCodeEditorItem((int)m_shaderStage[i]);
					}

					m_items.erase(m_items.begin() + i);
					delete m_editor[i];
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

	void CodeEditorUI::UpdateAutoRecompileItems()
	{
		if (m_contentChanged && m_lastAutoRecompile.GetElapsedTime() > 0.35f) {
			for (int i = 0; i < m_changedEditors.size(); i++) {

				for (int j = 0; j < m_editor.size(); j++) {
					if (m_editor[j] == m_changedEditors[i]) {
						if (m_items[j]->Type == PipelineItem::ItemType::ShaderPass) {
							std::string vs = "", ps = "", gs = "";
							if (m_shaderStage[j] == ShaderStage::Vertex)
								vs = m_editor[j]->GetText();
							else if (m_shaderStage[j] == ShaderStage::Pixel)
								ps = m_editor[j]->GetText();
							else if (m_shaderStage[j] == ShaderStage::Geometry)
								gs = m_editor[j]->GetText();
							m_data->Renderer.RecompileFromSource(m_items[j]->Name, vs, ps, gs);
						} else if (m_items[j]->Type == PipelineItem::ItemType::ComputePass)
							m_data->Renderer.RecompileFromSource(m_items[j]->Name, m_editor[j]->GetText());
						else if (m_items[j]->Type == PipelineItem::ItemType::AudioPass)
							m_data->Renderer.RecompileFromSource(m_items[j]->Name, m_editor[j]->GetText());
						else if (m_items[j]->Type == PipelineItem::ItemType::PluginItem) {
							std::string pluginCode = m_editor[j]->GetText();
							((pipe::PluginItemData*)m_items[j]->Data)->Owner->HandleRecompileFromSource(m_items[j]->Name, (int)m_shaderStage[j], pluginCode.c_str(), pluginCode.size());
						}

						break;
					}
				}
			}

			m_changedEditors.clear();
			m_lastAutoRecompile.Restart();
			m_contentChanged = false;
		}
	}

	void CodeEditorUI::SetTheme(const TextEditor::Palette& colors)
	{
		for (TextEditor* editor : m_editor)
			editor->SetPalette(colors);
	}
	void CodeEditorUI::ApplySettings()
	{
		for (TextEditor* editor : m_editor) {

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
		}

		SetFont(Settings::Instance().Editor.Font, Settings::Instance().Editor.FontSize);
		SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange); 
	}
	void CodeEditorUI::SetFont(const std::string& filename, int size)
	{
		m_fontNeedsUpdate = m_fontFilename != filename || m_fontSize != size;
		m_fontFilename = filename;
		m_fontSize = size;
	}

	void CodeEditorUI::Open(PipelineItem* item, ShaderStage stage)
	{
		std::string shaderPath = "";
		std::string shaderContent = "";

		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			ed::pipe::ShaderPass* shader = reinterpret_cast<ed::pipe::ShaderPass*>(item->Data);

			if (stage == ShaderStage::Vertex)
				shaderPath = shader->VSPath;
			else if (stage == ShaderStage::Pixel)
				shaderPath = shader->PSPath;
			else if (stage == ShaderStage::Geometry)
				shaderPath = shader->GSPath;
		} else if (item->Type == PipelineItem::ItemType::ComputePass) {
			ed::pipe::ComputePass* shader = reinterpret_cast<ed::pipe::ComputePass*>(item->Data);
			shaderPath = shader->Path;
		} else if (item->Type == PipelineItem::ItemType::AudioPass) {
			ed::pipe::AudioPass* shader = reinterpret_cast<ed::pipe::AudioPass*>(item->Data);
			shaderPath = shader->Path;
		}

		shaderPath = m_data->Parser.GetProjectPath(shaderPath);

		if (Settings::Instance().General.UseExternalEditor) {
			UIHelper::ShellOpen(shaderPath);
			return;
		}

		// check if file is already opened
		for (int i = 0; i < m_paths.size(); i++) {
			if (m_paths[i] == shaderPath) {
				m_focusWindow = true;
				m_focusStage = stage;
				m_focusItem = m_items[i]->Name;
				return;
			}
		}

		shaderContent = m_data->Parser.LoadFile(shaderPath);

		m_items.push_back(item);
		m_editor.push_back(new TextEditor());
		m_editorOpen.push_back(true);
		m_stats.push_back(StatsPage(m_ui, m_data, "", false));
		m_shaderStage.push_back(stage);
		m_paths.push_back(shaderPath);

		TextEditor* editor = m_editor[m_editor.size() - 1];

		editor->OnContentUpdate = [&](TextEditor* chEditor) {
			if (Settings::Instance().General.AutoRecompile) {
				if (std::count(m_changedEditors.begin(), m_changedEditors.end(), chEditor) == 0)
					m_changedEditors.push_back(chEditor);
				m_contentChanged = true;
			}
		};

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

		ShaderLanguage sLang = ShaderCompiler::GetShaderLanguageFromExtension(shaderPath);
		
		if (sLang == ShaderLanguage::HLSL)
			editor->SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
		else if (sLang == ShaderLanguage::Plugin) {
			int langID = 0;
			IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, shaderPath, m_data->Plugins.Plugins());
			editor->SetLanguageDefinition(m_buildLanguageDefinition(plugin, langID));
		}
		else
			editor->SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

		// apply breakpoints
		m_applyBreakpoints(editor, shaderPath);

		editor->SetText(shaderContent);
		editor->ResetTextChanged();
	}
	void CodeEditorUI::OpenPluginCode(PipelineItem* item, const char* filepath, int id)
	{
		if (item->Type != PipelineItem::ItemType::PluginItem)
			return;

		pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(item->Data);

		if (Settings::Instance().General.UseExternalEditor) {
			std::string path = m_data->Parser.GetProjectPath(filepath);
			UIHelper::ShellOpen(path);
			return;
		}

		// check if already opened
		for (int i = 0; i < m_paths.size(); i++) {
			if (m_paths[i] == filepath) {
				m_focusWindow = true;
				m_focusStage = (ShaderStage)id; // TODO: this might not work anymore
				m_focusItem = m_items[i]->Name;
				return;
			}
		}

		ShaderStage shaderStage = (ShaderStage)id;

		// TODO: some of this can be moved outside of the if() {} block
		m_items.push_back(item);
		m_editor.push_back(new TextEditor());
		m_editorOpen.push_back(true);
		m_stats.push_back(StatsPage(m_ui, m_data, "", false));
		m_paths.push_back(filepath);
		m_shaderStage.push_back(shaderStage);

		TextEditor* editor = m_editor[m_editor.size() - 1];

		TextEditor::LanguageDefinition defPlugin = m_buildLanguageDefinition(shader->Owner, id);

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

		std::string shaderContent = m_data->Parser.LoadProjectFile(filepath);
		editor->SetText(shaderContent);
		editor->ResetTextChanged();
	}
	TextEditor* CodeEditorUI::Get(PipelineItem* item, ShaderStage stage)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == item && m_shaderStage[i] == stage)
				return m_editor[i];
		return nullptr;
	}

	void CodeEditorUI::CloseAll(PipelineItem* item)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (m_items[i] == item || item == nullptr) {
				if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[i]->Data);
					shader->Owner->CloseCodeEditorItem((int)m_shaderStage[i]);
				}

				delete m_editor[i];

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
			ret.push_back(m_editor[i]->GetText());
		return ret;
	}
	void CodeEditorUI::SetOpenedFilesData(const std::vector<std::string>& data)
	{
		for (int i = 0; i < m_items.size() && i < data.size(); i++)
			m_editor[i]->SetText(data[i]);
	}
	void CodeEditorUI::m_loadEditorShortcuts(TextEditor* ed)
	{
		auto sMap = KeyboardShortcuts::Instance().GetMap();

		for (auto it = sMap.begin(); it != sMap.end(); it++) {
			std::string id = it->first;

			if (id.size() > 8 && id.substr(0, 6) == "Editor") {
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
		const std::vector<dbg::Breakpoint>& bkpts = m_data->Debugger.GetBreakpointList(path);
		const std::vector<bool>& states = m_data->Debugger.GetBreakpointStateList(path);

		for (size_t i = 0; i < bkpts.size(); i++)
			editor->AddBreakpoint(bkpts[i].Line, bkpts[i].IsConditional ? bkpts[i].Condition : "", states[i]);

		editor->OnBreakpointRemove = [&](TextEditor* ed, int line) {
			m_data->Debugger.RemoveBreakpoint(ed->GetPath(), line);
		};
		editor->OnBreakpointUpdate = [&](TextEditor* ed, int line, const std::string& cond, bool enabled) {
			m_data->Debugger.AddBreakpoint(ed->GetPath(), line, cond, enabled);
		};
	}

	void CodeEditorUI::StopDebugging()
	{
		for (auto& ed : m_editor)
			ed->SetCurrentLineIndicator(-1);
	}
	void CodeEditorUI::StopThreads()
	{
		m_trackerRunning = false;
		if (m_trackThread)
			m_trackThread->detach();
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
		} else {
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

		std::vector<std::string> allFiles;	// list of all files we care for
		std::vector<std::string> allPasses; // list of shader pass names that correspond to the file name
		std::vector<std::string> paths;		// list of all paths that we should have "notifications turned on"

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
			std::this_thread::sleep_for(std::chrono::milliseconds(25));

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
					} else
						foundGS = true;

					if (!foundGS || !foundVS || !foundPS) {
						needsUpdate = true;
						break;
					}
				} else if (pass->Type == PipelineItem::ItemType::ComputePass) {
					pipe::ComputePass* data = (pipe::ComputePass*)pass->Data;

					bool found = false;

					std::string path(m_data->Parser.GetProjectPath(data->Path));

					for (auto& f : allFiles)
						if (f == path)
							found = true;

					if (!found) {
						needsUpdate = true;
						break;
					}
				} else if (pass->Type == PipelineItem::ItemType::AudioPass) {
					pipe::AudioPass* data = (pipe::AudioPass*)pass->Data;

					bool found = false;

					std::string path(m_data->Parser.GetProjectPath(data->Path));

					for (auto& f : allFiles)
						if (f == path)
							found = true;

					if (!found) {
						needsUpdate = true;
						break;
					}
				} else if (pass->Type == PipelineItem::ItemType::PluginItem) {
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
					} else if (pass->Type == PipelineItem::ItemType::ComputePass) {
						pipe::ComputePass* data = (pipe::ComputePass*)pass->Data;

						std::string path(m_data->Parser.GetProjectPath(data->Path));

						allFiles.push_back(path);
						paths.push_back(path.substr(0, path.find_last_of("/\\") + 1));
						allPasses.push_back(pass->Name);
					} else if (pass->Type == PipelineItem::ItemType::AudioPass) {
						pipe::AudioPass* data = (pipe::AudioPass*)pass->Data;

						std::string path(m_data->Parser.GetProjectPath(data->Path));

						allFiles.push_back(path);
						paths.push_back(path.substr(0, path.find_last_of("/\\") + 1));
						allPasses.push_back(pass->Name);
					} else if (pass->Type == PipelineItem::ItemType::PluginItem) {
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
		}
#elif defined(__linux__) || defined(__unix__)
			fd_set rfds;
			int eCount = select(notifyEngine + 1, &rfds, NULL, NULL, NULL);

			if (eCount <= 0) continue;

			// check for changes
			bufLength = read(notifyEngine, buffer, EVENT_BUF_LEN);
			if (bufLength < 0) { /* TODO: error! */
			}

			// read all events
			while (bufIndex < bufLength) {
				struct inotify_event* event = (struct inotify_event*)&buffer[bufIndex];
				if (event->len) {
					if (event->mask & IN_MODIFY) {
						if (event->mask & IN_ISDIR) { /* it is a directory - do nothing */
						} else {
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
					FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
					&bytesReturned,
					&pOverlap[i],
					NULL);
			}

			DWORD dwWaitStatus = WaitForMultipleObjects(paths.size(), events.data(), false, 1000);

			// check if we got any info
			if (dwWaitStatus != WAIT_TIMEOUT) {
				bufferOffset = 0;
				do {
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

	TextEditor::LanguageDefinition CodeEditorUI::m_buildLanguageDefinition(IPlugin1* plugin, int languageID)
	{
		TextEditor::LanguageDefinition langDef;

		int keywordCount = plugin->GetLanguageDefinitionKeywordCount(languageID);
		const char** keywords = plugin->GetLanguageDefinitionKeywords(languageID);

		for (int i = 0; i < keywordCount; i++)
			langDef.mKeywords.insert(keywords[i]);

		int identifierCount = plugin->GetLanguageDefinitionIdentifierCount(languageID);
		for (int i = 0; i < identifierCount; i++) {
			const char* ident = plugin->GetLanguageDefinitionIdentifier(i, languageID);
			const char* identDesc = plugin->GetLanguageDefinitionIdentifierDesc(i, languageID);
			langDef.mIdentifiers.insert(std::make_pair(ident, TextEditor::Identifier(identDesc)));
		}
		// m_GLSLDocumentation(langDef.mIdentifiers);

		int tokenRegexs = plugin->GetLanguageDefinitionTokenRegexCount(languageID);
		for (int i = 0; i < tokenRegexs; i++) {
			plugin::TextEditorPaletteIndex palIndex;
			const char* regStr = plugin->GetLanguageDefinitionTokenRegex(i, palIndex, languageID);
			langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>(regStr, (TextEditor::PaletteIndex)palIndex));
		}

		langDef.mCommentStart = plugin->GetLanguageDefinitionCommentStart(languageID);
		langDef.mCommentEnd = plugin->GetLanguageDefinitionCommentEnd(languageID);
		langDef.mSingleLineComment = plugin->GetLanguageDefinitionLineComment(languageID);

		langDef.mCaseSensitive = plugin->IsLanguageDefinitionCaseSensitive(languageID);
		langDef.mAutoIndentation = plugin->GetLanguageDefinitionAutoIndent(languageID);

		langDef.mName = plugin->GetLanguageDefinitionName(languageID);

		return langDef;
	}
}