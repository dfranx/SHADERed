#include <SHADERed/Objects/KeyboardShortcuts.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/PreviewUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <misc/ImFileDialog.h>

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
			, m_trackUpdatesNeeded(0)
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
	void CodeEditorUI::m_save(int editor_id)
	{
		if (editor_id >= m_editor.size())
			return;

		m_editorSaveRequestID = editor_id;

		std::function<void(bool)> saveFunc = [&](bool success) {
			if (success) {
				PluginShaderEditor* pluginEd = &m_pluginEditor[m_editorSaveRequestID];
				TextEditor* ed = m_editor[m_editorSaveRequestID];
				std::string text = "";
				if (ed == nullptr) {
					size_t textLength = 0;

					const char* tempText = pluginEd->Plugin->ShaderEditor_GetContent(pluginEd->LanguageID, pluginEd->ID, &textLength);
					text = std::string(tempText, textLength);
				} else
					text = ed->GetText();

				bool isSeparateFile = m_items[m_editorSaveRequestID] == nullptr;

				std::string path = "";

				// TODO: why not just m_paths[m_editorSaveRequestID] ? was this written before m_paths was added?
				if (!isSeparateFile) {
					if (m_items[m_editorSaveRequestID]->Type == PipelineItem::ItemType::ShaderPass) {
						ed::pipe::ShaderPass* shader = reinterpret_cast<ed::pipe::ShaderPass*>(m_items[m_editorSaveRequestID]->Data);

						if (m_shaderStage[m_editorSaveRequestID] == ShaderStage::Vertex)
							path = shader->VSPath;
						else if (m_shaderStage[m_editorSaveRequestID] == ShaderStage::Pixel)
							path = shader->PSPath;
						else if (m_shaderStage[m_editorSaveRequestID] == ShaderStage::Geometry)
							path = shader->GSPath;
						else if (m_shaderStage[m_editorSaveRequestID] == ShaderStage::TessellationControl)
							path = shader->TCSPath;
						else if (m_shaderStage[m_editorSaveRequestID] == ShaderStage::TessellationEvaluation)
							path = shader->TESPath;
					} else if (m_items[m_editorSaveRequestID]->Type == PipelineItem::ItemType::ComputePass) {
						ed::pipe::ComputePass* shader = reinterpret_cast<ed::pipe::ComputePass*>(m_items[m_editorSaveRequestID]->Data);
						path = shader->Path;
					} else if (m_items[m_editorSaveRequestID]->Type == PipelineItem::ItemType::AudioPass) {
						ed::pipe::AudioPass* shader = reinterpret_cast<ed::pipe::AudioPass*>(m_items[m_editorSaveRequestID]->Data);
						path = shader->Path;
					}
				} else
					path = m_paths[m_editorSaveRequestID];

				if (ed)
					ed->ResetTextChanged();
				else
					pluginEd->Plugin->ShaderEditor_ResetChangeState(pluginEd->LanguageID, pluginEd->ID);

				if (!isSeparateFile && m_items[m_editorSaveRequestID]->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[m_editorSaveRequestID]->Data);
					std::string edsrc = m_editor[m_editorSaveRequestID]->GetText();
					shader->Owner->CodeEditor_SaveItem(edsrc.c_str(), edsrc.size(), m_paths[m_editorSaveRequestID].c_str()); // TODO: custom stages
				} else
					m_data->Parser.SaveProjectFile(path, text);
			} else {
				// do nothing
			}
		};

		// prompt user to choose a project location first
		if (m_data->Parser.GetOpenedFile() == "") {
			RequestedProjectSave = true;
			m_ui->SaveAsProject(true, saveFunc);
		} else
			saveFunc(true);
	}
	void CodeEditorUI::m_saveAsSPV(int id)
	{
		if (id < m_items.size()) {
			m_editorSaveRequestID = id;
			ifd::FileDialog::Instance().Save("SaveSPVBinaryDlg", "Save SPIR-V binary", "SPIR-V binary (*.spv){.spv},.*");
		}
	}
	void CodeEditorUI::m_saveAsGLSL(int id)
	{
		if (id < m_items.size()) {
			m_editorSaveRequestID = id;
			ifd::FileDialog::Instance().Save("SaveGLSLDlg", "Save as GLSL", "GLSL source (*.glsl){.glsl},.*");
		}
	}
	void CodeEditorUI::m_saveAsHLSL(int id)
	{
		if (id < m_items.size()) {
			m_editorSaveRequestID = id;
			ifd::FileDialog::Instance().Save("SaveHLSLDlg", "Save as HLSL", "HLSL source (*.hlsl){.hlsl},.*");
		}
	}
	void CodeEditorUI::m_compile(int id)
	{
		if (id >= m_editor.size() || m_items[id] == nullptr)
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
			else if (m_shaderStage[id] == ShaderStage::TessellationControl)
				shaderFile = shader->TCSPath;
			else if (m_shaderStage[id] == ShaderStage::TessellationEvaluation)
				shaderFile = shader->TESPath;
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
		int wid[10] = { 0 }; // vs, ps, gs, cs, tes, tcs, pl

		// editor windows
		for (int i = 0; i < m_editor.size(); i++) {
			if (m_editorOpen[i]) {
				bool isSeparateFile = (m_items[i] == nullptr);
				bool isPluginItem = isSeparateFile ? false : (m_items[i]->Type == PipelineItem::ItemType::PluginItem);
				pipe::PluginItemData* plData = isSeparateFile ? nullptr : (pipe::PluginItemData*)m_items[i]->Data;

				std::string stageAbbr = "VS";
				if (m_shaderStage[i] == ShaderStage::Pixel) stageAbbr = "PS";
				else if (m_shaderStage[i] == ShaderStage::Geometry) stageAbbr = "GS";
				else if (m_shaderStage[i] == ShaderStage::Compute) stageAbbr = "CS";
				else if (m_shaderStage[i] == ShaderStage::Audio) stageAbbr = "AS";
				else if (m_shaderStage[i] == ShaderStage::TessellationControl) stageAbbr = "TCS";
				else if (m_shaderStage[i] == ShaderStage::TessellationEvaluation) stageAbbr = "TES";
				else if (isSeparateFile) stageAbbr = "FILE";


				std::string shaderType = isPluginItem ? plData->Owner->LanguageDefinition_GetNameAbbreviation((int)m_shaderStage[i]) : stageAbbr;
				std::string windowName(std::string(isSeparateFile ? std::filesystem::path(m_paths[i]).filename().u8string() : m_items[i]->Name) + " (" + shaderType + ")");

				int pluginLanguageID = m_pluginEditor[i].LanguageID;
				int pluginEditorID = m_pluginEditor[i].ID;
				bool isPluginEditorChanged = m_pluginEditor[i].Plugin != nullptr && m_pluginEditor[i].Plugin->ShaderEditor_IsChanged(pluginLanguageID, pluginEditorID);
				bool isTextEditorChanged = m_editor[i] && m_editor[i]->IsTextChanged();
				if ((isTextEditorChanged || isPluginEditorChanged) && !m_data->Parser.IsProjectModified())
					m_data->Parser.ModifyProject();

				ImGui::SetNextWindowSizeConstraints(ImVec2(300, 300), ImVec2(10000, 10000));
				ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
				if (ImGui::Begin((std::string(windowName) + "###code_view" + shaderType + std::to_string(wid[isPluginItem ? 4 : (int)m_shaderStage[i]])).c_str(), &m_editorOpen[i], (ImGuiWindowFlags_UnsavedDocument * (isTextEditorChanged || isPluginEditorChanged)) | ImGuiWindowFlags_MenuBar)) {
					if (ImGui::BeginMenuBar()) {
						if (ImGui::BeginMenu("File")) {
							if (ImGui::MenuItem("Save", KeyboardShortcuts::Instance().GetString("CodeUI.Save").c_str())) m_save(i);
							if (ImGui::MenuItem("Save SPIR-V binary")) m_saveAsSPV(i);
							if (ImGui::MenuItem("Save as GLSL")) m_saveAsGLSL(i);
							if (ImGui::MenuItem("Save as HLSL")) m_saveAsHLSL(i);
							ImGui::EndMenu();
						}
						if (ImGui::BeginMenu("Code")) {
							if (ImGui::MenuItem("Compile", KeyboardShortcuts::Instance().GetString("CodeUI.Compile").c_str(), false, m_items[i] != nullptr)) m_compile(i);
							
							bool showStats = m_items[i] != nullptr && (m_pluginEditor[i].Plugin == nullptr || (m_pluginEditor[i].Plugin && m_pluginEditor[i].Plugin->ShaderEditor_HasStats(pluginLanguageID, pluginEditorID)));
							
							if (showStats) {
								if (!m_stats[i].Visible && ImGui::MenuItem("Stats", KeyboardShortcuts::Instance().GetString("CodeUI.SwitchView").c_str())) {
									m_stats[i].Visible = true;
									m_stats[i].Refresh(m_items[i], m_shaderStage[i]);
								}
								if (m_stats[i].Visible && ImGui::MenuItem("Code", KeyboardShortcuts::Instance().GetString("CodeUI.SwitchView").c_str())) m_stats[i].Visible = false;
							}

							if (m_editor[i]) {
								ImGui::Separator();
								if (ImGui::MenuItem("Undo", "CTRL+Z", nullptr, m_editor[i]->CanUndo())) m_editor[i]->Undo();
								if (ImGui::MenuItem("Redo", "CTRL+Y", nullptr, m_editor[i]->CanRedo())) m_editor[i]->Redo();
								ImGui::Separator();
								if (ImGui::MenuItem("Cut", "CTRL+X")) m_editor[i]->Cut();
								if (ImGui::MenuItem("Copy", "CTRL+C")) m_editor[i]->Copy();
								if (ImGui::MenuItem("Paste", "CTRL+V")) m_editor[i]->Paste();
								if (ImGui::MenuItem("Select All", "CTRL+A")) m_editor[i]->SelectAll();
							} else {
								ImGui::Separator();
								if (ImGui::MenuItem("Undo", "CTRL+Z", nullptr, m_pluginEditor[i].Plugin->ShaderEditor_CanUndo(pluginLanguageID, pluginEditorID))) m_pluginEditor[i].Plugin->ShaderEditor_Undo(pluginLanguageID, pluginEditorID);
								if (ImGui::MenuItem("Redo", "CTRL+Y", nullptr, m_pluginEditor[i].Plugin->ShaderEditor_CanRedo(pluginLanguageID, pluginEditorID))) m_pluginEditor[i].Plugin->ShaderEditor_Redo(pluginLanguageID, pluginEditorID);
								ImGui::Separator();
								if (ImGui::MenuItem("Cut", "CTRL+X")) m_pluginEditor[i].Plugin->ShaderEditor_Cut(pluginLanguageID, pluginEditorID);
								if (ImGui::MenuItem("Copy", "CTRL+C")) m_pluginEditor[i].Plugin->ShaderEditor_Copy(pluginLanguageID, pluginEditorID);
								if (ImGui::MenuItem("Paste", "CTRL+V")) m_pluginEditor[i].Plugin->ShaderEditor_Paste(pluginLanguageID, pluginEditorID);
								if (ImGui::MenuItem("Select All", "CTRL+A")) m_pluginEditor[i].Plugin->ShaderEditor_SelectAll(pluginLanguageID, pluginEditorID);
							}

							ImGui::EndMenu();
						}

						ImGui::EndMenuBar();
					}

					if (m_stats[i].Visible) {
						ImGui::PushFont(m_font);
						m_stats[i].Update(delta);
						ImGui::PopFont();
					} else {
						if (m_editor[i]) {
							DrawTextEditor(windowName, m_editor[i]);
						} else {
							PluginShaderEditor* pluginData = &m_pluginEditor[i];
							if (pluginData->Plugin) {
								ImGui::PushFont(m_font);
								pluginData->Plugin->ShaderEditor_Render(pluginData->LanguageID, pluginData->ID);
								ImGui::PopFont();
							}
						}
					}

					if (m_editor[i] && m_editor[i]->IsFocused())
						m_selectedItem = i;
				}

				if (m_focusWindow) {
					if (m_focusPath == m_paths[i]) {
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
			if (!m_editorOpen[i] && (m_editor[i] && m_editor[i]->IsTextChanged())) {
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

		// save spir-v binary dialog
		if (ifd::FileDialog::Instance().IsDone("SaveSPVBinaryDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) {
				std::string filePathName = ifd::FileDialog::Instance().GetResult().u8string();

				std::vector<unsigned int> spv;

				PipelineItem* item = m_items[m_editorSaveRequestID];
				ShaderStage stage = m_shaderStage[m_editorSaveRequestID];

				if (item->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* pass = (pipe::ShaderPass*)item->Data;
					if (stage == ShaderStage::Pixel)
						spv = pass->PSSPV;
					else if (stage == ShaderStage::Vertex)
						spv = pass->VSSPV;
					else if (stage == ShaderStage::Geometry)
						spv = pass->GSSPV;
					else if (stage == ShaderStage::TessellationControl)
						spv = pass->TCSSPV;
					else if (stage == ShaderStage::TessellationEvaluation)
						spv = pass->TESSPV;
				} else if (item->Type == PipelineItem::ItemType::ComputePass) {
					pipe::ComputePass* pass = (pipe::ComputePass*)item->Data;
					if (stage == ShaderStage::Pixel)
						spv = pass->SPV;
				} else if (item->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* data = (pipe::PluginItemData*)item->Data;
					unsigned int spvSize = data->Owner->PipelineItem_GetSPIRVSize(data->Type, data->PluginData, (plugin::ShaderStage)stage);
					unsigned int* spvPtr = data->Owner->PipelineItem_GetSPIRV(data->Type, data->PluginData, (plugin::ShaderStage)stage);

					if (spvPtr != nullptr && spvSize != 0)
						spv = std::vector<unsigned int>(spvPtr, spvPtr + spvSize);
				}

				std::ofstream spvOut(filePathName, std::ios::out | std::ios::binary);
				spvOut.write((char*)spv.data(), spv.size() * sizeof(unsigned int));
				spvOut.close();
			}
			ifd::FileDialog::Instance().Close();
		}

		// save glsl dialog
		if (ifd::FileDialog::Instance().IsDone("SaveGLSLDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) {
				std::string filePathName = ifd::FileDialog::Instance().GetResult().u8string();

				std::vector<unsigned int> spv;
				bool gsUsed = false;
				bool tsUsed = false;

				PipelineItem* item = m_items[m_editorSaveRequestID];
				ShaderStage stage = m_shaderStage[m_editorSaveRequestID];

				ed::ShaderLanguage lang = ShaderCompiler::GetShaderLanguageFromExtension(m_paths[m_editorSaveRequestID]);
				
				if (item->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* pass = (pipe::ShaderPass*)item->Data;
					if (stage == ShaderStage::Pixel)
						spv = pass->PSSPV;
					else if (stage == ShaderStage::Vertex)
						spv = pass->VSSPV;
					else if (stage == ShaderStage::Geometry)
						spv = pass->GSSPV;
					else if (stage == ShaderStage::TessellationControl)
						spv = pass->TCSSPV;
					else if (stage == ShaderStage::TessellationEvaluation)
						spv = pass->TESSPV;

					gsUsed = pass->GSUsed;
					tsUsed = pass->TSUsed;
				} else if (item->Type == PipelineItem::ItemType::ComputePass) {
					pipe::ComputePass* pass = (pipe::ComputePass*)item->Data;
					if (stage == ShaderStage::Pixel)
						spv = pass->SPV;
				} else if (item->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* data = (pipe::PluginItemData*)item->Data;
					unsigned int spvSize = data->Owner->PipelineItem_GetSPIRVSize(data->Type, data->PluginData, (plugin::ShaderStage)stage);
					unsigned int* spvPtr = data->Owner->PipelineItem_GetSPIRV(data->Type, data->PluginData, (plugin::ShaderStage)stage);

					if (spvPtr != nullptr && spvSize != 0)
						spv = std::vector<unsigned int>(spvPtr, spvPtr + spvSize);
				}

				std::string glslSource = ed::ShaderCompiler::ConvertToGLSL(spv, lang, stage, tsUsed, gsUsed, nullptr);

				std::ofstream spvOut(filePathName, std::ios::out | std::ios::binary);
				spvOut.write(glslSource.c_str(), glslSource.size());
				spvOut.close();
			}
			ifd::FileDialog::Instance().Close();
		}

		// save hlsl dialog
		if (ifd::FileDialog::Instance().IsDone("SaveHLSLDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) {
				std::string filePathName = ifd::FileDialog::Instance().GetResult().u8string();

				std::vector<unsigned int> spv;

				PipelineItem* item = m_items[m_editorSaveRequestID];
				ShaderStage stage = m_shaderStage[m_editorSaveRequestID];

				if (item->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* pass = (pipe::ShaderPass*)item->Data;
					if (stage == ShaderStage::Pixel)
						spv = pass->PSSPV;
					else if (stage == ShaderStage::Vertex)
						spv = pass->VSSPV;
					else if (stage == ShaderStage::Geometry)
						spv = pass->GSSPV;
					else if (stage == ShaderStage::TessellationControl)
						spv = pass->TCSSPV;
					else if (stage == ShaderStage::TessellationEvaluation)
						spv = pass->TESSPV;
				} else if (item->Type == PipelineItem::ItemType::ComputePass) {
					pipe::ComputePass* pass = (pipe::ComputePass*)item->Data;
					if (stage == ShaderStage::Pixel)
						spv = pass->SPV;
				} else if (item->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* data = (pipe::PluginItemData*)item->Data;
					unsigned int spvSize = data->Owner->PipelineItem_GetSPIRVSize(data->Type, data->PluginData, (plugin::ShaderStage)stage);
					unsigned int* spvPtr = data->Owner->PipelineItem_GetSPIRV(data->Type, data->PluginData, (plugin::ShaderStage)stage);

					if (spvPtr != nullptr && spvSize != 0)
						spv = std::vector<unsigned int>(spvPtr, spvPtr + spvSize);
				}

				std::string hlslSource = ed::ShaderCompiler::ConvertToHLSL(spv, stage);

				std::ofstream spvOut(filePathName, std::ios::out | std::ios::binary);
				spvOut.write(hlslSource.c_str(), hlslSource.size());
				spvOut.close();
			}
			ifd::FileDialog::Instance().Close();
		}

		// delete closed editors
		if (m_savePopupOpen == -1) {
			for (int i = 0; i < m_editorOpen.size(); i++) {
				if (!m_editorOpen[i]) {
					if (m_items[i] && m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[i]->Data);
						shader->Owner->CodeEditor_CloseItem(m_paths[i].c_str());
					}

					IPlugin1* plugin = m_pluginEditor[i].Plugin;
					if (plugin)
						plugin->ShaderEditor_Close(m_pluginEditor[i].LanguageID, m_pluginEditor[i].ID);

					m_items.erase(m_items.begin() + i);
					delete m_editor[i];
					m_editor.erase(m_editor.begin() + i);
					m_pluginEditor.erase(m_pluginEditor.begin() + i);
					m_editorOpen.erase(m_editorOpen.begin() + i);
					m_stats.erase(m_stats.begin() + i);
					m_paths.erase(m_paths.begin() + i);
					m_shaderStage.erase(m_shaderStage.begin() + i);
					i--;
				}
			}
		}
	}

	void CodeEditorUI::DrawTextEditor(const std::string& windowName, TextEditor* tEdit)
	{
		if (tEdit) {
			int i = 0;
			for (; i < m_editor.size(); i++)
				if (m_editor[i] == tEdit)
					break;

			// add error markers if needed
			auto msgs = m_data->Messages.GetMessages();
			int groupMsg = 0;
			TextEditor::ErrorMarkers groupErrs;
			for (int j = 0; j < msgs.size(); j++) {
				ed::MessageStack::Message* msg = &msgs[j];

				if (groupErrs.count(msg->Line))
					continue;

				if (m_items[i] != nullptr && msg->Line > 0 && msg->Group == m_items[i]->Name && msg->Shader == m_shaderStage[i])
					groupErrs[msg->Line] = msg->Text;
			}
			tEdit->SetErrorMarkers(groupErrs);

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
	}

	void CodeEditorUI::LoadSnippets()
	{
		Logger::Get().Log("Loading code snippets");

		std::string snippetsFileLoc = Settings::Instance().ConvertPath("data/snippets.xml");

		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(snippetsFileLoc.c_str());
		if (!result)
			return;

		m_snippets.clear();

		pugi::xml_node snippetsList = doc.child("snippets");
		for (pugi::xml_node snippetNode : snippetsList.children("snippet")) {
			CodeSnippet snippet;

			snippet.Language = snippetNode.child("language").text().as_string();
			snippet.Display = snippetNode.child("display").text().as_string();
			snippet.Search = snippetNode.child("search").text().as_string();
			snippet.Code = snippetNode.child("code").text().as_string();

			m_snippets.push_back(snippet);
		}
	}
	void CodeEditorUI::SaveSnippets()
	{
		pugi::xml_document doc;
		pugi::xml_node snippetsList = doc.append_child("snippets");

		for (const auto& snippet : m_snippets) {
			pugi::xml_node snippetNode = snippetsList.append_child("snippet");

			snippetNode.append_child("language").text().set(snippet.Language.c_str());
			snippetNode.append_child("display").text().set(snippet.Display.c_str());
			snippetNode.append_child("search").text().set(snippet.Search.c_str());
			snippetNode.append_child("code").text().set(snippet.Code.c_str());
		}

		std::string snippetsFileLoc = Settings::Instance().ConvertPath("data/snippets.xml");
		doc.save_file(snippetsFileLoc.c_str());
	}
	void CodeEditorUI::AddSnippet(const std::string& lang, const std::string& display, const std::string& search, const std::string& code)
	{
		RemoveSnippet(lang, display);

		CodeSnippet snip;
		snip.Language = lang;
		snip.Display = display;
		snip.Search = search;
		snip.Code = code;
		m_snippets.push_back(snip);
	}
	void CodeEditorUI::RemoveSnippet(const std::string& lang, const std::string& display)
	{
		for (int i = 0; i < m_snippets.size(); i++)
			if (m_snippets[i].Language == lang && m_snippets[i].Display == display) {
				m_snippets.erase(m_snippets.begin() + i);
				i--;
			}
	}

	void CodeEditorUI::ChangePluginShaderEditor(IPlugin1* plugin, int langID, int editorID)
	{
		PluginShaderEditor* plEditor = nullptr;

		if (Settings::Instance().General.AutoRecompile) {
			for (int i = 0; i < m_pluginEditor.size(); i++) {
				if (m_pluginEditor[i].Plugin != nullptr)
					if (m_pluginEditor[i].LanguageID == langID && m_pluginEditor[i].ID == editorID &&
						m_pluginEditor[i].Plugin == plugin)
						plEditor = &m_pluginEditor[i];
			}
		}

		if (plEditor) {
			if (std::count(m_changedPluginEditors.begin(), m_changedPluginEditors.end(), plEditor) == 0)
				m_changedPluginEditors.push_back(plEditor);
			m_contentChanged = true;
		}
	}
	PipelineItem* CodeEditorUI::GetPluginEditorPipelineItem(IPlugin1* plugin, int langID, int editorID)
	{
		PluginShaderEditor* plEditor = nullptr;
		
		for (int i = 0; i < m_pluginEditor.size(); i++) {
			if (m_pluginEditor[i].LanguageID == langID && m_pluginEditor[i].ID == editorID && m_pluginEditor[i].Plugin == plugin)
				return m_items[i];
		}

		return nullptr;
	}
	void CodeEditorUI::UpdateAutoRecompileItems()
	{
		if (m_contentChanged && m_lastAutoRecompile.GetElapsedTime() > 0.8f) {
			for (int i = 0; i < m_changedEditors.size(); i++) {
				for (int j = 0; j < m_editor.size(); j++) {
					if (m_editor[j] == m_changedEditors[i] && m_items[j] != nullptr) {
						if (m_items[j]->Type == PipelineItem::ItemType::ShaderPass) {
							std::string vs = "", ps = "", gs = "", tcs = "", tes = "";
							if (m_shaderStage[j] == ShaderStage::Vertex)
								vs = m_editor[j]->GetText();
							else if (m_shaderStage[j] == ShaderStage::Pixel)
								ps = m_editor[j]->GetText();
							else if (m_shaderStage[j] == ShaderStage::Geometry)
								gs = m_editor[j]->GetText();
							else if (m_shaderStage[j] == ShaderStage::TessellationControl)
								tcs = m_editor[j]->GetText();
							else if (m_shaderStage[j] == ShaderStage::TessellationEvaluation)
								tes = m_editor[j]->GetText();
							m_data->Renderer.RecompileFromSource(m_items[j]->Name, vs, ps, gs, tcs, tes);
						}
						else if (m_items[j]->Type == PipelineItem::ItemType::ComputePass)
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
			for (int i = 0; i < m_changedPluginEditors.size(); i++) {
				for (int j = 0; j < m_pluginEditor.size(); j++) {
					int langID = m_changedPluginEditors[i]->LanguageID;
					int editorID = m_changedPluginEditors[i]->ID;
					IPlugin1* plugin = m_changedPluginEditors[i]->Plugin;

					if (langID == m_pluginEditor[j].LanguageID &&
						editorID == m_pluginEditor[j].ID &&
						plugin == m_pluginEditor[j].Plugin)
					{
						size_t contentLength = 0;
						const char* tempText = plugin->ShaderEditor_GetContent(langID, editorID, &contentLength);

						if (m_items[j]->Type == PipelineItem::ItemType::ShaderPass) {
							std::string vs = "", ps = "", gs = "", tcs = "", tes = "";
							if (m_shaderStage[j] == ShaderStage::Vertex)
								vs = std::string(tempText, contentLength);
							else if (m_shaderStage[j] == ShaderStage::Pixel)
								ps = std::string(tempText, contentLength);
							else if (m_shaderStage[j] == ShaderStage::Geometry)
								gs = std::string(tempText, contentLength);
							else if (m_shaderStage[j] == ShaderStage::TessellationControl)
								tcs = std::string(tempText, contentLength);
							else if (m_shaderStage[j] == ShaderStage::TessellationEvaluation)
								tes = std::string(tempText, contentLength);
							m_data->Renderer.RecompileFromSource(m_items[j]->Name, vs, ps, gs, tcs, tes);
						} else if (m_items[j]->Type == PipelineItem::ItemType::ComputePass)
							m_data->Renderer.RecompileFromSource(m_items[j]->Name, std::string(tempText, contentLength));
						else if (m_items[j]->Type == PipelineItem::ItemType::AudioPass)
							m_data->Renderer.RecompileFromSource(m_items[j]->Name, std::string(tempText, contentLength));
						else if (m_items[j]->Type == PipelineItem::ItemType::PluginItem) {
							std::string pluginCode = std::string(tempText, contentLength);
							((pipe::PluginItemData*)m_items[j]->Data)->Owner->HandleRecompileFromSource(m_items[j]->Name, (int)m_shaderStage[j], pluginCode.c_str(), pluginCode.size());
						}

						break;
					}
				}
			}

			m_changedPluginEditors.clear();
			m_changedEditors.clear();
			m_lastAutoRecompile.Restart();
			m_contentChanged = false;
		}
	}

	void CodeEditorUI::SetTheme(const TextEditor::Palette& colors)
	{
		for (TextEditor* editor : m_editor)
			if (editor)
				editor->SetPalette(colors);
	}
	void CodeEditorUI::ApplySettings()
	{
		const std::vector<IPlugin1*>& plugins = m_data->Plugins.Plugins();
		for (int i = 0; i < m_editor.size(); i++) {
			TextEditor* editor = m_editor[i];
			if (editor == nullptr)
				continue; 
			editor->SetTabSize(Settings::Instance().Editor.TabSize);
			editor->SetInsertSpaces(Settings::Instance().Editor.InsertSpaces);
			editor->SetSmartIndent(Settings::Instance().Editor.SmartIndent);
			editor->SetAutoIndentOnPaste(Settings::Instance().Editor.AutoIndentOnPaste);
			editor->SetShowWhitespaces(Settings::Instance().Editor.ShowWhitespace);
			editor->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
			editor->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
			editor->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
			editor->SetHorizontalScroll(Settings::Instance().Editor.HorizontalScroll);
			editor->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
			editor->SetFunctionTooltips(Settings::Instance().Editor.FunctionTooltips);
			editor->SetFunctionDeclarationTooltip(Settings::Instance().Editor.FunctionDeclarationTooltips);
			editor->SetUIScale(Settings::Instance().TempScale);
			editor->SetUIFontSize(Settings::Instance().General.FontSize);
			editor->SetEditorFontSize(Settings::Instance().Editor.FontSize);
			editor->SetActiveAutocomplete(Settings::Instance().Editor.ActiveSmartPredictions);
			editor->SetColorizerEnable(Settings::Instance().Editor.SyntaxHighlighting);
			editor->SetScrollbarMarkers(Settings::Instance().Editor.ScrollbarMarkers);
			editor->SetHiglightBrackets(Settings::Instance().Editor.HighlightBrackets);
			editor->SetFoldEnabled(Settings::Instance().Editor.CodeFolding);
			
			editor->ClearAutocompleteEntries();

			plugin::ShaderStage stage = (plugin::ShaderStage)m_shaderStage[i];
			for (IPlugin1* plugin : plugins) {
				for (int i = 0; i < plugin->Autocomplete_GetCount(stage); i++) {
					const char* displayString = plugin->Autocomplete_GetDisplayString(stage, i);
					const char* searchString = plugin->Autocomplete_GetSearchString(stage, i);
					const char* value = plugin->Autocomplete_GetValue(stage, i);

					editor->AddAutocompleteEntry(searchString, displayString, value);
				}
			}
			for (const auto& snippet : m_snippets)
				if (editor->GetLanguageDefinition().mName == snippet.Language || snippet.Language == "*")
					editor->AddAutocompleteEntry(snippet.Search, snippet.Display, snippet.Code);

			m_loadEditorShortcuts(editor);
		}

		SetFont(Settings::Instance().Editor.Font, Settings::Instance().Editor.FontSize);
		SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange); 
	}
	void CodeEditorUI::FillAutocomplete(TextEditor* tEdit, const SPIRVParser& spv, bool colorize)
	{
		bool changed = false;

		if (colorize) {
			// check if there are any function changes
			for (const auto& func : spv.Functions) {
				bool funcExists = false;
				for (const auto& editor : tEdit->GetAutocompleteFunctions()) {
					if (editor.first == func.first) {
						funcExists = true;

						// check for argument changes
						for (const auto& arg : func.second.Arguments) {
							bool argExists = false;
							for (const auto& editorArg : editor.second.Arguments) {
								if (arg.Name == editorArg.Name) {
									argExists = true;
									break;
								}
							}
							if (!argExists) changed = true;
						}

						// check for local variable changes
						for (const auto& loc : func.second.Locals) {
							bool locExists = false;
							for (const auto& editorLoc : editor.second.Locals) {
								if (loc.Name == editorLoc.Name) {
									locExists = true;
									break;
								}
							}
							if (!locExists) changed = true;
						}

						break;
					}
				}

				if (!funcExists) changed = true;
			}
			// check if there are any user type changes
			for (const auto& type : spv.UserTypes) {
				bool typeExists = false;
				for (const auto& editor : tEdit->GetAutocompleteUserTypes()) {
					if (type.first == editor.first) {
						typeExists = true;
						break;
					}
				}
				if (!typeExists) changed = true;
			}
			// check if there are any uniform var changes
			for (const auto& unif : spv.Uniforms) {
				bool unifExists = false;
				for (const auto& editor : tEdit->GetAutocompleteUniforms()) {
					if (unif.Name == editor.Name) {
						unifExists = true;
						break;
					}
				}
				if (!unifExists) changed = true;
			}
			// check if there are any global var changes
			for (const auto& glob : spv.Globals) {
				bool globExists = false;
				for (const auto& editor : tEdit->GetAutocompleteGlobals()) {
					if (glob.Name == editor.Name) {
						globExists = true;
						break;
					}
				}
				if (!globExists) changed = true;
			}
		}

		// pass the data to text editor
		tEdit->ClearAutocompleteData();
		tEdit->ClearAutocompleteEntries();

		// spirv parser
		tEdit->SetAutocompleteFunctions(spv.Functions);
		tEdit->SetAutocompleteUserTypes(spv.UserTypes);
		tEdit->SetAutocompleteUniforms(spv.Uniforms);
		tEdit->SetAutocompleteGlobals(spv.Globals);

		// plugins
		plugin::ShaderStage stage = plugin::ShaderStage::Vertex;
		for (int i = 0; i < m_editor.size(); i++)
			if (m_editor[i] == tEdit) {
				stage = (plugin::ShaderStage)m_shaderStage[i];
				break;
			}
		const std::vector<IPlugin1*>& plugins = m_data->Plugins.Plugins();
		for (IPlugin1* plugin : plugins) {
			for (int i = 0; i < plugin->Autocomplete_GetCount(stage); i++) {
				const char* displayString = plugin->Autocomplete_GetDisplayString(stage, i);
				const char* searchString = plugin->Autocomplete_GetSearchString(stage, i);
				const char* value = plugin->Autocomplete_GetValue(stage, i);

				tEdit->AddAutocompleteEntry(searchString, displayString, value);
			}
		}

		// snippets
		for (const auto& snippet : m_snippets)
			if (tEdit->GetLanguageDefinition().mName == snippet.Language || snippet.Language == "*")
				tEdit->AddAutocompleteEntry(snippet.Search, snippet.Display, snippet.Code);

		// colorize if needed
		if (changed)
			tEdit->Colorize();
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
		bool externalEditor = Settings::Instance().General.UseExternalEditor;
		SPIRVParser spvData;

		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			ed::pipe::ShaderPass* shader = reinterpret_cast<ed::pipe::ShaderPass*>(item->Data);

			if (stage == ShaderStage::Vertex) {
				shaderPath = shader->VSPath;
				if (!externalEditor) spvData.Parse(shader->VSSPV);
			} else if (stage == ShaderStage::Pixel) {
				shaderPath = shader->PSPath;
				if (!externalEditor) spvData.Parse(shader->PSSPV);
			} else if (stage == ShaderStage::Geometry) {
				shaderPath = shader->GSPath;
				if (!externalEditor) spvData.Parse(shader->GSSPV);
			} else if (stage == ShaderStage::TessellationControl) {
				shaderPath = shader->TCSPath;
				if (!externalEditor) spvData.Parse(shader->TCSSPV);
			} else if (stage == ShaderStage::TessellationEvaluation) {
				shaderPath = shader->TESPath;
				if (!externalEditor) spvData.Parse(shader->TESSPV);
			}
		} else if (item->Type == PipelineItem::ItemType::ComputePass) {
			ed::pipe::ComputePass* shader = reinterpret_cast<ed::pipe::ComputePass*>(item->Data);
			shaderPath = shader->Path;
			if (!externalEditor) spvData.Parse(shader->SPV);
		} else if (item->Type == PipelineItem::ItemType::AudioPass) {
			ed::pipe::AudioPass* shader = reinterpret_cast<ed::pipe::AudioPass*>(item->Data);
			shaderPath = shader->Path;
		}

		shaderPath = m_data->Parser.GetProjectPath(shaderPath);

		if (externalEditor) {
			UIHelper::ShellOpen(shaderPath);
			return;
		}

		// check if file is already opened
		for (int i = 0; i < m_paths.size(); i++) {
			if (m_paths[i] == shaderPath) {
				m_focusWindow = true;
				m_focusPath = shaderPath;
				return;
			}
		}

		int langID = 0;
		IPlugin1* plugin = nullptr;
		ShaderLanguage sLang = ShaderCompiler::GetShaderLanguageFromExtension(shaderPath);
		if (sLang == ShaderLanguage::Plugin)
			plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, shaderPath, m_data->Plugins.Plugins());

		shaderContent = m_data->Parser.LoadFile(shaderPath);

		m_items.push_back(item);
		m_editorOpen.push_back(true);
		m_stats.push_back(StatsPage(m_ui, m_data, "", false));
		m_shaderStage.push_back(stage);
		m_paths.push_back(shaderPath);
		m_pluginEditor.push_back(PluginShaderEditor());

		if (plugin != nullptr && plugin->ShaderEditor_Supports(langID))
			m_editor.push_back(nullptr);
		else
			m_editor.push_back(new TextEditor());

		TextEditor* editor = m_editor[m_editor.size() - 1];

		if (editor != nullptr) {
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
			editor->SetAutoIndentOnPaste(Settings::Instance().Editor.AutoIndentOnPaste);
			editor->SetShowWhitespaces(Settings::Instance().Editor.ShowWhitespace);
			editor->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
			editor->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
			editor->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
			editor->SetHorizontalScroll(Settings::Instance().Editor.HorizontalScroll);
			editor->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
			editor->SetFunctionTooltips(Settings::Instance().Editor.FunctionTooltips);
			editor->SetFunctionDeclarationTooltip(Settings::Instance().Editor.FunctionDeclarationTooltips);
			editor->SetPath(shaderPath);
			editor->SetUIScale(Settings::Instance().DPIScale);
			editor->SetUIFontSize(Settings::Instance().General.FontSize);
			editor->SetEditorFontSize(Settings::Instance().Editor.FontSize);
			editor->SetActiveAutocomplete(Settings::Instance().Editor.ActiveSmartPredictions);
			editor->SetColorizerEnable(Settings::Instance().Editor.SyntaxHighlighting);
			editor->SetScrollbarMarkers(Settings::Instance().Editor.ScrollbarMarkers);
			editor->SetHiglightBrackets(Settings::Instance().Editor.HighlightBrackets);
			editor->SetFoldEnabled(Settings::Instance().Editor.CodeFolding);
			editor->RequestOpen = [&](TextEditor* tEdit, const std::string& tEditPath, const std::string& path) {
				OpenFile(m_findIncludedFile(tEditPath, path));
			};
			editor->OnCtrlAltClick = [&](TextEditor* tEdit, const std::string& keyword, TextEditor::Coordinates coords) {
				for (int t = 0; t < m_editor.size(); t++)
					if (m_editor[t] == tEdit) {
						((PreviewUI*)m_ui->Get(ViewID::Preview))->SetVariableValue(m_items[t], keyword, coords.mLine);
						break;
					}
			};
			m_loadEditorShortcuts(editor);

			if (sLang == ShaderLanguage::HLSL)
				editor->SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
			else if (sLang == ShaderLanguage::Plugin) {
				if (plugin->LanguageDefinition_Exists(langID))
					editor->SetLanguageDefinition(m_buildLanguageDefinition(plugin, langID));
			} else
				editor->SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

			// apply breakpoints
			m_applyBreakpoints(editor, shaderPath);

			editor->SetText(shaderContent);
			editor->ResetTextChanged();

			FillAutocomplete(editor, spvData, false);
		} else {
			int idMax = -1;
			for (int i = 0; i < m_pluginEditor.size(); i++)
				idMax = std::max<int>(m_pluginEditor[i].ID, idMax);

			PluginShaderEditor* pluginEditor = &m_pluginEditor[m_pluginEditor.size()-1];
			pluginEditor->LanguageID = langID;
			pluginEditor->Plugin = plugin;
			pluginEditor->ID = idMax + 1;

			m_setupPlugin(pluginEditor->Plugin);

			pluginEditor->Plugin->ShaderEditor_Open(pluginEditor->LanguageID, pluginEditor->ID, shaderContent.c_str(), shaderContent.size());
		}
	}
	void CodeEditorUI::OpenFile(const std::string& path)
	{
		std::string shaderContent = "";
		bool externalEditor = Settings::Instance().General.UseExternalEditor;
		
		if (externalEditor) {
			UIHelper::ShellOpen(path);
			return;
		}

		// check if file is already opened
		for (int i = 0; i < m_paths.size(); i++) {
			if (m_paths[i] == path) {
				m_focusWindow = true;
				m_focusPath = path;
				return;
			}
		}

		int langID = 0;
		IPlugin1* plugin = nullptr;
		ShaderLanguage sLang = ShaderCompiler::GetShaderLanguageFromExtension(path);
		if (sLang == ShaderLanguage::Plugin)
			plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, path, m_data->Plugins.Plugins());

		shaderContent = m_data->Parser.LoadFile(path);

		m_items.push_back(nullptr);
		m_editorOpen.push_back(true);
		m_stats.push_back(StatsPage(m_ui, m_data, "", false));
		m_shaderStage.push_back(ShaderStage::Count);
		m_paths.push_back(path);
		m_pluginEditor.push_back(PluginShaderEditor());

		if (plugin != nullptr && plugin->ShaderEditor_Supports(langID))
			m_editor.push_back(nullptr);
		else
			m_editor.push_back(new TextEditor());

		TextEditor* editor = m_editor[m_editor.size() - 1];

		if (editor != nullptr) {
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
			editor->SetAutoIndentOnPaste(Settings::Instance().Editor.AutoIndentOnPaste);
			editor->SetShowWhitespaces(Settings::Instance().Editor.ShowWhitespace);
			editor->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
			editor->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
			editor->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
			editor->SetHorizontalScroll(Settings::Instance().Editor.HorizontalScroll);
			editor->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
			editor->SetFunctionTooltips(Settings::Instance().Editor.FunctionTooltips);
			editor->SetFunctionDeclarationTooltip(Settings::Instance().Editor.FunctionDeclarationTooltips);
			editor->SetPath(path);
			editor->SetUIScale(Settings::Instance().DPIScale);
			editor->SetUIFontSize(Settings::Instance().General.FontSize);
			editor->SetEditorFontSize(Settings::Instance().Editor.FontSize);
			editor->SetActiveAutocomplete(Settings::Instance().Editor.ActiveSmartPredictions);
			editor->SetColorizerEnable(Settings::Instance().Editor.SyntaxHighlighting);
			editor->SetScrollbarMarkers(Settings::Instance().Editor.ScrollbarMarkers);
			editor->SetHiglightBrackets(Settings::Instance().Editor.HighlightBrackets);
			editor->SetFoldEnabled(Settings::Instance().Editor.CodeFolding);
			editor->RequestOpen = [&](TextEditor* tEdit, const std::string& tEditPath, const std::string& path) {
				OpenFile(m_findIncludedFile(tEditPath, path));
			};
			editor->OnCtrlAltClick = [&](TextEditor* tEdit, const std::string& keyword, TextEditor::Coordinates coords) {
				for (int t = 0; t < m_editor.size(); t++)
					if (m_editor[t] == tEdit) {
						((PreviewUI*)m_ui->Get(ViewID::Preview))->SetVariableValue(m_items[t], keyword, coords.mLine);
						break;
					}
			};
			m_loadEditorShortcuts(editor);

			if (sLang == ShaderLanguage::HLSL)
				editor->SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
			else if (sLang == ShaderLanguage::Plugin) {
				if (plugin->LanguageDefinition_Exists(langID))
					editor->SetLanguageDefinition(m_buildLanguageDefinition(plugin, langID));
			} else
				editor->SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		} else {
			int idMax = -1;
			for (int i = 0; i < m_pluginEditor.size(); i++)
				idMax = std::max<int>(m_pluginEditor[i].ID, idMax);

			PluginShaderEditor* pluginEditor = &m_pluginEditor[m_pluginEditor.size() - 1];
			pluginEditor->LanguageID = langID;
			pluginEditor->Plugin = plugin;
			pluginEditor->ID = idMax + 1;

			m_setupPlugin(pluginEditor->Plugin);

			pluginEditor->Plugin->ShaderEditor_Open(pluginEditor->LanguageID, pluginEditor->ID, shaderContent.c_str(), shaderContent.size());
		}
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
				m_focusPath = filepath;
				return;
			}
		}

		ShaderStage shaderStage = (ShaderStage)id;

		int langID = 0;
		IPlugin1* plugin = nullptr;
		ShaderLanguage sLang = ShaderCompiler::GetShaderLanguageFromExtension(filepath);
		if (sLang == ShaderLanguage::Plugin)
			plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, filepath, m_data->Plugins.Plugins());

		m_items.push_back(item);
		m_editorOpen.push_back(true);
		m_stats.push_back(StatsPage(m_ui, m_data, "", false));
		m_paths.push_back(filepath);
		m_shaderStage.push_back(shaderStage);
		m_pluginEditor.push_back(PluginShaderEditor());
		
		if (plugin != nullptr && plugin->ShaderEditor_Supports(langID))
			m_editor.push_back(nullptr);
		else
			m_editor.push_back(new TextEditor());

		TextEditor* editor = m_editor[m_editor.size() - 1];
		std::string shaderContent = m_data->Parser.LoadProjectFile(filepath);

		if (editor != nullptr) {
			editor->OnContentUpdate = [&](TextEditor* chEditor) {
				if (Settings::Instance().General.AutoRecompile) {
					if (std::count(m_changedEditors.begin(), m_changedEditors.end(), chEditor) == 0)
						m_changedEditors.push_back(chEditor);
					m_contentChanged = true;
				}
			};

			TextEditor::LanguageDefinition defPlugin = m_buildLanguageDefinition(shader->Owner, id);

			editor->SetPalette(ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme));
			editor->SetTabSize(Settings::Instance().Editor.TabSize);
			editor->SetInsertSpaces(Settings::Instance().Editor.InsertSpaces);
			editor->SetSmartIndent(Settings::Instance().Editor.SmartIndent);
			editor->SetAutoIndentOnPaste(Settings::Instance().Editor.AutoIndentOnPaste);
			editor->SetShowWhitespaces(Settings::Instance().Editor.ShowWhitespace);
			editor->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
			editor->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
			editor->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
			editor->SetHorizontalScroll(Settings::Instance().Editor.HorizontalScroll);
			editor->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
			editor->SetFunctionTooltips(Settings::Instance().Editor.FunctionTooltips);
			editor->SetFunctionDeclarationTooltip(Settings::Instance().Editor.FunctionDeclarationTooltips);
			editor->SetPath(filepath);
			editor->SetUIScale(Settings::Instance().DPIScale);
			editor->SetUIFontSize(Settings::Instance().General.FontSize);
			editor->SetEditorFontSize(Settings::Instance().Editor.FontSize);
			editor->SetActiveAutocomplete(Settings::Instance().Editor.ActiveSmartPredictions);
			editor->SetColorizerEnable(Settings::Instance().Editor.SyntaxHighlighting);
			editor->SetScrollbarMarkers(Settings::Instance().Editor.ScrollbarMarkers);
			editor->SetHiglightBrackets(Settings::Instance().Editor.HighlightBrackets);
			editor->SetFoldEnabled(Settings::Instance().Editor.CodeFolding);
			editor->RequestOpen = [&](TextEditor* tEdit, const std::string& tEditPath, const std::string& path) {
				OpenFile(m_findIncludedFile(tEditPath, path));
			};
			editor->OnCtrlAltClick = [&](TextEditor* tEdit, const std::string& keyword, TextEditor::Coordinates coords) {
				for (int t = 0; t < m_editor.size(); t++)
					if (m_editor[t] == tEdit) {
						((PreviewUI*)m_ui->Get(ViewID::Preview))->SetVariableValue(m_items[t], keyword, coords.mLine);
						break;
					}
			};
			m_loadEditorShortcuts(editor);

			unsigned int spvSize = shader->Owner->PipelineItem_GetSPIRVSize(shader->Type, shader->PluginData, (plugin::ShaderStage)shaderStage);
			if (spvSize > 0) {
				unsigned int* spv = shader->Owner->PipelineItem_GetSPIRV(shader->Type, shader->PluginData, (plugin::ShaderStage)shaderStage);
				std::vector<unsigned int> spvVec(spv, spv + spvSize);

				SPIRVParser spvData;
				spvData.Parse(spvVec);
				FillAutocomplete(editor, spvData, false);
			}

			// apply breakpoints
			m_applyBreakpoints(editor, filepath);

			editor->SetLanguageDefinition(defPlugin);

			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		} else {
			int idMax = -1;
			for (int i = 0; i < m_pluginEditor.size(); i++)
				idMax = std::max<int>(m_pluginEditor[i].ID, idMax);

			PluginShaderEditor* pluginEditor = &m_pluginEditor[m_pluginEditor.size() - 1];
			pluginEditor->LanguageID = langID;
			pluginEditor->Plugin = plugin;
			pluginEditor->ID = idMax + 1;

			m_setupPlugin(pluginEditor->Plugin);

			pluginEditor->Plugin->ShaderEditor_Open(pluginEditor->LanguageID, pluginEditor->ID, shaderContent.c_str(), shaderContent.size());
		}
	}
	TextEditor* CodeEditorUI::Get(PipelineItem* item, ShaderStage stage)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == item && m_shaderStage[i] == stage)
				return m_editor[i];
		return nullptr;
	}
	TextEditor* CodeEditorUI::Get(const std::string& path)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_paths[i] == path)
				return m_editor[i];
		return nullptr;
	}

	PluginShaderEditor CodeEditorUI::GetPluginEditor(PipelineItem* item, ed::ShaderStage stage)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == item && m_shaderStage[i] == stage)
				return m_pluginEditor[i];
		return PluginShaderEditor();
	}
	PluginShaderEditor CodeEditorUI::GetPluginEditor(const std::string& path)
	{
		for (int i = 0; i < m_paths.size(); i++)
			if (m_paths[i] == path)
				return m_pluginEditor[i];
		return PluginShaderEditor();
	}
	std::string CodeEditorUI::GetPluginEditorPath(const PluginShaderEditor& editor)
	{
		for (int i = 0; i < m_pluginEditor.size(); i++)
			if (m_pluginEditor[i].ID == editor.ID && m_pluginEditor[i].LanguageID == editor.LanguageID &&
				m_pluginEditor[i].Plugin == editor.Plugin)
				return m_paths[i];
		return "";
	}

	void CodeEditorUI::CloseAll(PipelineItem* item)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (m_items[i] == item || item == nullptr) {
				if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* shader = reinterpret_cast<pipe::PluginItemData*>(m_items[i]->Data);
					shader->Owner->CodeEditor_CloseItem(m_paths[i].c_str());
				}

				delete m_editor[i];

				m_items.erase(m_items.begin() + i);
				m_editor.erase(m_editor.begin() + i);
				m_pluginEditor.erase(m_pluginEditor.begin() + i);
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
			if (m_items[i] != nullptr)
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
	std::string CodeEditorUI::m_findIncludedFile(const std::string& relativeTo, const std::string& path)
	{
		for (const auto& p : Settings::Instance().Project.IncludePaths) {
			std::filesystem::path ret = std::filesystem::path(p) / path;

			if (!ret.is_absolute()) {
				std::string projectPath = m_data->Parser.GetProjectPath(ret.u8string());
				if (std::filesystem::exists(projectPath))
					return projectPath;
			}

			if (std::filesystem::exists(ret))
				return ret.u8string();
		}

		std::filesystem::path ret = std::filesystem::path(relativeTo);
		if (ret.has_parent_path())
			ret = ret.parent_path() / path;
		if (std::filesystem::exists(ret))
			return ret.u8string();

		return path;
	}
	void CodeEditorUI::m_applyBreakpoints(TextEditor* editor, const std::string& path)
	{
		const std::vector<dbg::Breakpoint>& bkpts = m_data->Debugger.GetBreakpointList(path);
		const std::vector<bool>& states = m_data->Debugger.GetBreakpointStateList(path);

		for (size_t i = 0; i < bkpts.size(); i++)
			editor->AddBreakpoint(bkpts[i].Line, bkpts[i].IsConditional, bkpts[i].Condition, states[i]);

		editor->OnBreakpointRemove = [&](TextEditor* ed, int line) {
			m_data->Debugger.RemoveBreakpoint(ed->GetPath(), line);
		};
		editor->OnBreakpointUpdate = [&](TextEditor* ed, int line, bool useCond, const std::string& cond, bool enabled) {
			m_data->Debugger.AddBreakpoint(ed->GetPath(), line, useCond, cond, enabled);
		};
	}

	void CodeEditorUI::m_setupPlugin(ed::IPlugin1* plugin)
	{
		plugin->OnEditorContentChange = [](void* UI, void* plugin, int langID, int editorID) {
			GUIManager* gui = (GUIManager*)UI;
			CodeEditorUI* code = ((CodeEditorUI*)gui->Get(ViewID::Code));
			code->ChangePluginShaderEditor((IPlugin1*)plugin, langID, editorID);
		};

		if (plugin->GetVersion() >= 3) {
			IPlugin3* plug3 = (IPlugin3*)plugin;
			plug3->GetEditorPipelineItem = [](void* UI, void* plugin, int langID, int editorID) -> void* {
				GUIManager* gui = (GUIManager*)UI;
				CodeEditorUI* code = ((CodeEditorUI*)gui->Get(ViewID::Code));
				return (void*)code->GetPluginEditorPipelineItem((IPlugin1*)plugin, langID, editorID);
			};
		}
	}

	void CodeEditorUI::StopDebugging()
	{
		for (int i = 0; i < m_editor.size(); i++) {
			if (m_editor[i])
				m_editor[i]->SetCurrentLineIndicator(-1);
			else {
				if (m_pluginEditor[i].Plugin->GetVersion() >= 3)
					((IPlugin3*)m_pluginEditor[i].Plugin)->ShaderEditor_SetLineIndicator(m_pluginEditor[i].LanguageID,
						m_pluginEditor[i].ID, -1);
			}
		}
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
		std::vector<bool> tsUsed(passes.size());

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
		char filename[SHADERED_MAX_PATH];
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

					bool foundVS = false, foundPS = false, foundGS = false, foundTCS = false, foundTES = false;

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

					if (data->TSUsed) {
						std::string tcsPath(m_data->Parser.GetProjectPath(data->TCSPath));
						std::string tesPath(m_data->Parser.GetProjectPath(data->TESPath));
						for (auto& f : allFiles) {
							if (f == tesPath) {
								foundTES = true;
								break;
							}
							if (f == tcsPath) {
								foundTCS = true;
								break;
							}
						}
					} else {
						foundTES = true;
						foundTCS = true;
					}
					

					if (!foundGS || !foundVS || !foundPS || !foundTES || !foundTCS) {
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

					if (data->Owner->ShaderFilePath_HasChanged()) {
						needsUpdate = true;
						data->Owner->ShaderFilePath_Update();
					}
				}
			}

			for (int i = 0; i < gsUsed.size() && i < nPasses.size() && i < tsUsed.size(); i++) {
				if (nPasses[i]->Type == PipelineItem::ItemType::ShaderPass) {
					bool newGSUsed = ((pipe::ShaderPass*)nPasses[i]->Data)->GSUsed;
					bool newTSUsed = ((pipe::ShaderPass*)nPasses[i]->Data)->TSUsed;
					if (gsUsed[i] != newGSUsed) {
						gsUsed[i] = newGSUsed;
						needsUpdate = true;
					}
					if (tsUsed[i] != newTSUsed) {
						tsUsed[i] = newTSUsed;
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
				tsUsed.resize(passes.size());
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

						// geometry shader
						if (data->GSUsed) {
							std::string gsPath(m_data->Parser.GetProjectPath(data->GSPath));

							allFiles.push_back(gsPath);
							paths.push_back(gsPath.substr(0, gsPath.find_last_of("/\\") + 1));
							allPasses.push_back(pass->Name);
						}

						// tessellation shader
						if (data->TSUsed) {
							// tessellation control shader
							std::string tcsPath(m_data->Parser.GetProjectPath(data->TCSPath));
							allFiles.push_back(tcsPath);
							paths.push_back(tcsPath.substr(0, tcsPath.find_last_of("/\\") + 1));
							allPasses.push_back(pass->Name);

							// tessellation evaluation shader
							std::string tesPath(m_data->Parser.GetProjectPath(data->TESPath));
							allFiles.push_back(tesPath);
							paths.push_back(tesPath.substr(0, tesPath.find_last_of("/\\") + 1));
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

						int count = data->Owner->ShaderFilePath_GetCount();

						for (int i = 0; i < count; i++) {
							std::string path(m_data->Parser.GetProjectPath(data->Owner->ShaderFilePath_Get(i)));

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
							char filename[SHADERED_MAX_PATH];
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

		int keywordCount = plugin->LanguageDefinition_GetKeywordCount(languageID);
		const char** keywords = plugin->LanguageDefinition_GetKeywords(languageID);

		for (int i = 0; i < keywordCount; i++)
			langDef.mKeywords.insert(keywords[i]);

		int identifierCount = plugin->LanguageDefinition_GetIdentifierCount(languageID);
		for (int i = 0; i < identifierCount; i++) {
			const char* ident = plugin->LanguageDefinition_GetIdentifier(i, languageID);
			const char* identDesc = plugin->LanguageDefinition_GetIdentifierDesc(i, languageID);
			langDef.mIdentifiers.insert(std::make_pair(ident, TextEditor::Identifier(identDesc)));
		}
		// m_GLSLDocumentation(langDef.mIdentifiers);

		int tokenRegexs = plugin->LanguageDefinition_GetTokenRegexCount(languageID);
		for (int i = 0; i < tokenRegexs; i++) {
			plugin::TextEditorPaletteIndex palIndex;
			const char* regStr = plugin->LanguageDefinition_GetTokenRegex(i, palIndex, languageID);
			langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>(regStr, (TextEditor::PaletteIndex)palIndex));
		}

		langDef.mCommentStart = plugin->LanguageDefinition_GetCommentStart(languageID);
		langDef.mCommentEnd = plugin->LanguageDefinition_GetCommentEnd(languageID);
		langDef.mSingleLineComment = plugin->LanguageDefinition_GetLineComment(languageID);

		langDef.mCaseSensitive = plugin->LanguageDefinition_IsCaseSensitive(languageID);
		langDef.mAutoIndentation = plugin->LanguageDefinition_GetAutoIndent(languageID);

		langDef.mName = plugin->LanguageDefinition_GetName(languageID);

		return langDef;
	}
}