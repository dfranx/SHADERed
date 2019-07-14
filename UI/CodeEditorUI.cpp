#include "CodeEditorUI.h"
#include <imgui/imgui.h>
#include "../Objects/Names.h"
#include "../Objects/Settings.h"
#include "../Objects/GLSL2HLSL.h"
#include "../Objects/ThemeContainer.h"
#include "../Objects/KeyboardShortcuts.h"

#include <iostream>
#include <fstream>
#include <d3dcompiler.h>

#define STATUSBAR_HEIGHT 18 * Settings::Instance().DPIScale

const std::string EDITOR_SHORTCUT_NAMES[] =
{
	"Undo",
	"Redo",
	"MoveUp",
	"MoveDown",
	"MoveLeft",
	"MoveRight",
	"MoveTop",
	"MoveBottom",
	"MoveUpBlock",
	"MoveDownBlock",
	"MoveEndLine",
	"MoveStartLine",
	"ForwardDelete",
	"BackwardDelete",
	"OverwriteCursor",
	"Copy",
	"Paste",
	"Cut",
	"SelectAll",
	"AutocompleteOpen",
	"AutocompleteSelect",
	"AutocompleteSelectActive",
	"AutocompleteUp",
	"AutocompleteDown",
	"NewLine",
	"IndentShift"
};

namespace ed
{
	void CodeEditorUI::m_setupShortcuts() {
		KeyboardShortcuts::Instance().SetCallback("Editor.Compile", [=]() {
			if (m_selectedItem == -1)
				return;

			m_compile(m_selectedItem);
		});
		KeyboardShortcuts::Instance().SetCallback("Editor.Save", [=]() {
			if (m_selectedItem == -1)
				return;

			m_save(m_selectedItem);
		});
		KeyboardShortcuts::Instance().SetCallback("Editor.SwitchView", [=]() {
			if (m_selectedItem == -1)
				return;

			if (!m_stats[m_selectedItem].IsActive)
				m_stats[m_selectedItem].Fetch(&m_items[m_selectedItem], m_editor[m_selectedItem].GetText(), m_shaderTypeId[m_selectedItem]);
			else m_stats[m_selectedItem].IsActive = false;
		});
		KeyboardShortcuts::Instance().SetCallback("Editor.ToggleStatusbar", [=]() {
			Settings::Instance().Editor.StatusBar = !Settings::Instance().Editor.StatusBar;
		});
	}
	void CodeEditorUI::OnEvent(const ml::Event & e)
	{
	}
	void CodeEditorUI::Update(float delta)
	{
		if (m_editor.size() == 0)
			return;
		
		m_selectedItem = -1;

		// counters for each shader type for window ids
		int wid[3] = { 0, 0, 0 }; // vs, ps, gs

		// code editor windows
		for (int i = 0; i < m_editor.size(); i++) {
			if (m_editorOpen[i]) {
				std::string shaderType = m_shaderTypeId[i] == 0 ? "VS" : (m_shaderTypeId[i] == 1 ? "PS" : "GS");
				std::string windowName(std::string(m_items[i].Name) + " (" + shaderType + ")");
				
				ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
				if (ImGui::Begin((std::string(windowName) + "###code_view" + shaderType + std::to_string(wid[m_shaderTypeId[i]])).c_str(), &m_editorOpen[i], (ImGuiWindowFlags_UnsavedDocument * m_editor[i].IsTextChanged()) | ImGuiWindowFlags_MenuBar)) {
					if (ImGui::BeginMenuBar()) {
						if (ImGui::BeginMenu("File")) {
							if (ImGui::MenuItem("Save", KeyboardShortcuts::Instance().GetString("Editor.Save").c_str())) m_save(i);
							ImGui::EndMenu();
						}
						if (ImGui::BeginMenu("Code")) {
							if (ImGui::MenuItem("Compile", KeyboardShortcuts::Instance().GetString("Editor.Compile").c_str())) m_compile(i);
							if (!m_stats[i].IsActive && ImGui::MenuItem("Stats", KeyboardShortcuts::Instance().GetString("Editor.SwitchView").c_str())) m_stats[i].Fetch(&m_items[i], m_editor[i].GetText(), m_shaderTypeId[i]);
							if (m_stats[i].IsActive && ImGui::MenuItem("Code", KeyboardShortcuts::Instance().GetString("Editor.SwitchView").c_str())) m_stats[i].IsActive = false;
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
							if (msgs[j].Group == m_items[i].Name && msgs[j].Shader == m_shaderTypeId[i] && msgs[j].Line > 0)
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

							ed::pipe::ShaderPass* shader = reinterpret_cast<ed::pipe::ShaderPass*>(m_items[i].Data);
							char* path = shader->VSPath;
							if (m_shaderTypeId[i] == 1)
								path = shader->PSPath;
							else if (m_shaderTypeId[i] == 2)
								path = shader->GSPath;

							ImGui::Separator();
							ImGui::Text("Line %d\tCol %d\tType: %s\tPath: %s", cursor.mLine, cursor.mColumn, m_editor[i].GetLanguageDefinition().mName.c_str(), path);
						}
					}

					if (m_editor[i].IsFocused())
						m_selectedItem = i;
				}

				if (m_focusWindow) {
					if (m_focusItem == m_items[i].Name && m_focusSID == m_shaderTypeId[i]) {
						ImGui::SetWindowFocus();
						m_focusWindow = false;
					}
				}

				wid[m_shaderTypeId[i]]++;
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
					m_items.erase(m_items.begin() + i);
					m_editor.erase(m_editor.begin() + i);
					m_editorOpen.erase(m_editorOpen.begin() + i);
					m_stats.erase(m_stats.begin() + i);
					m_shaderTypeId.erase(m_shaderTypeId.begin() + i);
					i--;
				}
			}
		}
	}
	void CodeEditorUI::RenameShaderPass(const std::string& name, const std::string& newName)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (name == m_items[i].Name)
				strcpy(m_items[i].Name, newName.c_str());
	}
	void CodeEditorUI::m_open(PipelineItem item, int sid)
	{
		ed::pipe::ShaderPass* shader = reinterpret_cast<ed::pipe::ShaderPass*>(item.Data);

		if (Settings::Instance().General.UseExternalEditor) {
			std::string path = "";
			if (sid == 0)
				path = m_data->Parser.GetProjectPath(shader->VSPath);
			else if (sid == 1)
				path = m_data->Parser.GetProjectPath(shader->PSPath);
			else if (sid == 2)
				path = m_data->Parser.GetProjectPath(shader->GSPath);

			std::wstring wpath(path.begin(), path.end());

			ShellExecute(0, 0, wpath.c_str(), 0, 0, SW_SHOW);

			return;
		}

		// check if already opened
		for (int i = 0; i < m_items.size(); i++) {
			if (m_shaderTypeId[i] == sid) {
				ed::pipe::ShaderPass* sData = reinterpret_cast<ed::pipe::ShaderPass*>(m_items[i].Data);
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
					m_focusItem = m_items[i].Name;
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
		editor->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
		editor->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
		editor->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
		editor->SetHorizontalScroll(Settings::Instance().Editor.HorizontalScroll);
		editor->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
		m_loadEditorShortcuts(editor);

		bool isGLSL = false;
		if (sid == 0)
			isGLSL = GLSL2HLSL::IsGLSL(shader->VSPath);
		else if (sid == 1)
			isGLSL = GLSL2HLSL::IsGLSL(shader->PSPath);
		else if (sid == 2)
			isGLSL = GLSL2HLSL::IsGLSL(shader->GSPath);
		editor->SetLanguageDefinition(isGLSL ? defGLSL : defHLSL);
		
		m_shaderTypeId.push_back(sid);

		std::string shaderContent = "";
		if (sid == 0)
			shaderContent = m_data->Parser.LoadProjectFile(shader->VSPath);
		else if (sid == 1)
			shaderContent = m_data->Parser.LoadProjectFile(shader->PSPath);
		else if (sid == 2)
			shaderContent = m_data->Parser.LoadProjectFile(shader->GSPath);
		editor->SetText(shaderContent);
		editor->ResetTextChanged();
	}
	void CodeEditorUI::OpenVS(PipelineItem item)
	{
		m_open(item, 0);
	}
	void CodeEditorUI::OpenPS(PipelineItem item)
	{
		m_open(item, 1);
	}
	void CodeEditorUI::OpenGS(PipelineItem item)
	{
		m_open(item, 2);
	}
	void CodeEditorUI::CloseAll()
	{
		// delete not needed editors
		for (int i = 0; i < m_editorOpen.size(); i++) {
			m_items.erase(m_items.begin() + i);
			m_editor.erase(m_editor.begin() + i);
			m_editorOpen.erase(m_editorOpen.begin() + i);
			m_stats.erase(m_stats.begin() + i);
			m_shaderTypeId.erase(m_shaderTypeId.begin() + i);
			i--;
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
			ret.push_back(std::make_pair(std::string(m_items[i].Name), m_shaderTypeId[i]));
		return ret;
	}
	void CodeEditorUI::m_save(int id)
	{
		ed::pipe::ShaderPass* shader = reinterpret_cast<ed::pipe::ShaderPass*>(m_items[id].Data);

		m_editor[id].ResetTextChanged();

		if (m_shaderTypeId[id] == 0)
			m_data->Parser.SaveProjectFile(shader->VSPath, m_editor[id].GetText());
		else if (m_shaderTypeId[id] == 1)
			m_data->Parser.SaveProjectFile(shader->PSPath, m_editor[id].GetText());
		else if (m_shaderTypeId[id] == 2)
			m_data->Parser.SaveProjectFile(shader->GSPath, m_editor[id].GetText());
	}
	void CodeEditorUI::m_compile(int id)
	{
		m_save(id);

		m_data->Renderer.Recompile(m_items[id].Name);
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

	void CodeEditorUI::SetTrackFileChanges(bool track)
	{
		if (m_trackFileChanges == track)
			return;

		m_trackFileChanges = track;

		if (track) {
			if (m_trackThread != nullptr) {
				delete m_trackThread;
				m_trackThread = nullptr;
			}

			m_trackerRunning = true;

			m_trackThread = new std::thread(&CodeEditorUI::m_trackWorker, this);
		}
		else {
			m_trackerRunning = false;

			m_trackThread->join();
			delete m_trackThread;
			m_trackThread = nullptr;
		}
	}
	void CodeEditorUI::m_trackWorker()
	{
		std::string curProject = m_data->Parser.GetOpenedFile();

		std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
		std::vector<std::string> allFiles;		// list of all files we care for
		std::vector<std::string> allPasses;		// list of shader pass names that correspond to the file name
		std::vector<std::string> paths;			// list of all paths that we should have "notifications turned on"

		// variables for storing all the handles
		std::vector<HANDLE> events(paths.size());
		std::vector<HANDLE> hDirs(paths.size());
		std::vector<OVERLAPPED> pOverlap(paths.size());

		// buffer data
		const int bufferLen = 2048;
		char buffer[bufferLen];
		DWORD bytesReturned;
		char filename[MAX_PATH];

		// run this loop until we close the thread
		while (m_trackerRunning)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			// check if user added/changed shader paths
			std::vector<PipelineItem*> nPasses = m_data->Pipeline.GetList();
			bool needsUpdate = false;
			for (auto pass : nPasses) {
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

			// update our file collection if needed
			if (nPasses.size() != passes.size() || curProject != m_data->Parser.GetOpenedFile() || paths.size() == 0) {
				for (int i = 0; i < paths.size(); i++) {
					CloseHandle(hDirs[i]);
					CloseHandle(events[i]);
				}

				allFiles.clear();
				allPasses.clear();
				paths.clear();
				events.clear();
				hDirs.clear();
				pOverlap.clear();
				curProject = m_data->Parser.GetOpenedFile();
				
				// get all paths to all shaders
				passes = nPasses;
				for (auto pass : passes) {
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
			}

			if (paths.size() == 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
			}

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
							if (allFiles[i] == updatedFile)
								m_trackedShaderPasses.push_back(allPasses[i]);
					}

					bufferOffset += notif->NextEntryOffset;
				} while (notif->NextEntryOffset);
			}
		}
	}

	void CodeEditorUI::StatsPage::Fetch(ed::PipelineItem* item, const std::string& code, int typeId)
	{
		IsActive = true;

		pipe::ShaderPass* data = (pipe::ShaderPass*)item->Data;
		ID3DBlob *bytecodeBlob = nullptr, *errorBlob = nullptr;

		// get shader version
		std::string type = "ps_5_0";
		if (typeId == 0)
			type = "vs_5_0";
		else if (typeId == 2)
			type = "gs_5_0";

		// generate bytecode
		if (typeId == 0)
			D3DCompile(code.c_str(), code.size(), "shader.hlsl", nullptr, nullptr, data->VSEntry, type.c_str(), 0, 0, &bytecodeBlob, &errorBlob);
		else if (typeId == 1)
			D3DCompile(code.c_str(), code.size(), "shader.hlsl", nullptr, nullptr, data->PSEntry, type.c_str(), 0, 0, &bytecodeBlob, &errorBlob);
		else if (typeId == 2)
			D3DCompile(code.c_str(), code.size(), "shader.hlsl", nullptr, nullptr, data->GSEntry, type.c_str(), 0, 0, &bytecodeBlob, &errorBlob);

		// delete the error data, we dont need it
		if (errorBlob != nullptr) {
			std::string msg = std::string((char*)errorBlob->GetBufferPointer());
			m_data->GetOwner()->GetLogger()->Log(msg);

			errorBlob->Release();
			errorBlob = nullptr;

			if (bytecodeBlob != nullptr) {
				bytecodeBlob->Release();
				bytecodeBlob = nullptr;
			}

			IsActive = false;
			return;
		}

		// shader reflection
		D3DReflect(bytecodeBlob->GetBufferPointer(), bytecodeBlob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&Info);
	}
	void CodeEditorUI::StatsPage::Render()
	{
		//ImGui::PushFont(m_font);

		ID3D11ShaderReflection* shader = (ID3D11ShaderReflection*)Info;

		D3D11_SHADER_DESC shaderDesc;
		shader->GetDesc(&shaderDesc);
		
		ImGui::TextColored(ImVec4(1.0f, 0.89f, 0.71f, 1.0f), shaderDesc.Creator);
		ImGui::Text("Bound Resource Count: %d", shaderDesc.BoundResources);
		ImGui::Text("Temprorary Register Count: %d", shaderDesc.TempRegisterCount);
		ImGui::Text("Temprorary Array Count: %d", shaderDesc.TempArrayCount);
		ImGui::Text("Number of constant defines: %d", shaderDesc.DefCount);
		ImGui::Text("Declaration Count: %d", shaderDesc.DclCount);

		ImGui::NewLine();
		ImGui::Text("Constant Buffer Count: %d", shaderDesc.ConstantBuffers);
		ImGui::Separator();
		for (int i = 0; i < shaderDesc.ConstantBuffers; i++) {
			ID3D11ShaderReflectionConstantBuffer* cb = shader->GetConstantBufferByIndex(i);
			
			D3D11_SHADER_BUFFER_DESC cbDesc;
			cb->GetDesc(&cbDesc);

			ImGui::Text("%s:", cbDesc.Name);
			ImGui::SameLine();
			ImGui::Indent(160);
				ImGui::Text("Type: %s", (cbDesc.Type == D3D11_CT_CBUFFER) ? "scalar" : "texture");
				ImGui::Text("Size: %dB", cbDesc.Size);
				ImGui::Text("Variable Count: %d", cbDesc.Variables);
				for (int j = 0; j < cbDesc.Variables; j++) {
					ID3D11ShaderReflectionVariable* var = cb->GetVariableByIndex(j);

					D3D11_SHADER_VARIABLE_DESC varDesc;
					var->GetDesc(&varDesc);
					
					ImGui::Text("%s:", varDesc.Name);
					ImGui::SameLine();

					ImGui::Indent(160);
						D3D11_SHADER_TYPE_DESC typeDesc;
						var->GetType()->GetDesc(&typeDesc);
						ImGui::Text("Type: %s", typeDesc.Name);
						ImGui::Text("Size: %dB", varDesc.Size);
						ImGui::Text("Offset: %dB", varDesc.StartOffset);
					ImGui::Unindent(160);
				}
			ImGui::Unindent(160);
		}


		ImGui::NewLine();
		ImGui::Text("Instruction Count: %d", shaderDesc.InstructionCount);
		ImGui::Separator();
		ImGui::Indent(30);
			ImGui::Text("Non-categorized texture instructions: %d", shaderDesc.TextureNormalInstructions);
			ImGui::Text("Texture load instructions: %d", shaderDesc.TextureLoadInstructions);
			ImGui::Text("Texture comparison instructions: %d", shaderDesc.TextureCompInstructions);
			ImGui::Text("Texture bias instructions: %d", shaderDesc.TextureBiasInstructions);
			ImGui::Text("Texture gradient instructions: %d", shaderDesc.TextureGradientInstructions);
			ImGui::Text("Floating point arithmetic instructions: %d", shaderDesc.FloatInstructionCount);
			ImGui::Text("Signed integer arithmetic instructions: %d", shaderDesc.IntInstructionCount);
			ImGui::Text("Unsigned integer arithmetic instructions: %d", shaderDesc.UintInstructionCount);
			ImGui::Text("Static flow control instructions: %d", shaderDesc.StaticFlowControlCount);
			ImGui::Text("Dynamic flow control instructions: %d", shaderDesc.DynamicFlowControlCount);
			ImGui::Text("Macro instructions: %d", shaderDesc.MacroInstructionCount);
			ImGui::Text("Array instructions: %d", shaderDesc.ArrayInstructionCount);
			ImGui::Text("Cut instructions: %d", shaderDesc.CutInstructionCount);
			ImGui::Text("Emit instructions: %d", shaderDesc.EmitInstructionCount);
			ImGui::Text("Bitwise instructions: %d", shader->GetBitwiseInstructionCount());
			ImGui::Text("Conversion instructions: %d", shader->GetConversionInstructionCount());
			ImGui::Text("Movc instructions: %d", shader->GetMovcInstructionCount());
			ImGui::Text("Mov instructions: %d", shader->GetMovInstructionCount());
		ImGui::Unindent(30);

		//ImGui::PopFont();
	}
}