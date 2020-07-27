/*
MIT License

Copyright (c) 2019-2020 Stephane Cuillerdier (aka aiekick)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "ImGuiFileDialog.h"
#include <imgui/imgui.h>

#include <filesystem>
#include <fstream>

#include <string.h> // stricmp / strcasecmp
#include <sstream>
#include <iomanip>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#if defined(WIN32)
#define stat _stat
#define stricmp _stricmp
#include <cctype>
#include <ImGuiFileDialog/dirent.h>
#define PATH_SEP '\\'
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#define stricmp strcasecmp
#include <sys/types.h>
#include <dirent.h>
#define PATH_SEP '/'
#endif

#include <imgui/imgui.h>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui/imgui_internal.h>

#include <cstdlib>
#include <algorithm>
#include <iostream>

namespace igfd
{
	// Custom toggle button
	inline bool ToggleButton(const char *vLabel, bool *vToggled)
	{
		bool pressed = false;

		if (vToggled && *vToggled)
		{
			ImVec4 bua = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
			ImVec4 buh = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
			ImVec4 bu = ImGui::GetStyleColorVec4(ImGuiCol_Button);
			ImVec4 te = ImGui::GetStyleColorVec4(ImGuiCol_Text);
			ImGui::PushStyleColor(ImGuiCol_Button, bua);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, buh);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bu);
			ImGui::PushStyleColor(ImGuiCol_Text, te);
		}

		pressed = ImGui::Button(vLabel);

		if (vToggled && *vToggled)
		{
			ImGui::PopStyleColor(4);
		}

		if (vToggled && pressed)
			*vToggled = !*vToggled;

		return pressed;
	}

	static std::string s_fs_root = std::string(1u, PATH_SEP);

	// Helper functions
	inline int alphaSort(const struct dirent **a, const struct dirent **b)
	{
		return strcoll((*a)->d_name, (*b)->d_name);
	}
	inline bool replaceString(std::string& str, const std::string& oldStr, const std::string& newStr)
	{
		bool found = false;
		size_t pos = 0;
		while ((pos = str.find(oldStr, pos)) != std::string::npos)
		{
			found = true;
			str.replace(pos, oldStr.length(), newStr);
			pos += newStr.length();
		}
		return found;
	}
	inline std::vector<std::string> splitStringToVector(const std::string& text, char delimiter, bool pushEmpty)
	{
		std::vector<std::string> arr;
		if (!text.empty())
		{
			std::string::size_type start = 0;
			std::string::size_type end = text.find(delimiter, start);
			while (end != std::string::npos)
			{
				std::string token = text.substr(start, end - start);
				if (!token.empty() || (token.empty() && pushEmpty))
					arr.push_back(token);
				start = end + 1;
				end = text.find(delimiter, start);
			}
			std::string token = text.substr(start);
			if (token.size() > 0 || (token.empty() && pushEmpty))
				arr.push_back(token);
		}
		return arr;
	}
	inline std::vector<std::string> GetDrivesList()
	{
		std::vector<std::string> res;

#ifdef WIN32
		DWORD mydrives = 2048;
		char lpBuffer[2048];

		DWORD countChars = GetLogicalDriveStringsA(mydrives, lpBuffer);

		if (countChars > 0)
		{
			std::string var = std::string(lpBuffer, countChars);
			replaceString(var, "\\", "");
			res = splitStringToVector(var, '\0', false);
		}
#endif

		return res;
	}
	inline bool IsDirectoryExist(const std::string& name)
	{
		bool bExists = false;

		if (!name.empty())
		{
			DIR *pDir = nullptr;
			pDir = opendir(name.c_str());
			if (pDir != nullptr)
			{
				bExists = true;
				(void)closedir(pDir);
			}
		}

		return bExists;    // this is not a directory!
	}
	inline bool CreateDirectoryIfNotExist(const std::string& name)
	{
		bool res = false;

		if (!name.empty())
		{
			if (!IsDirectoryExist(name))
			{
				res = true;

#if defined(WIN32)
				CreateDirectoryA(name.c_str(), nullptr);
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
				char buffer[PATH_MAX] = {};
				snprintf(buffer, PATH_MAX, "mkdir -p %s", name.c_str());
				const int dir_err = std::system(buffer);
				if (dir_err == -1)
				{
					std::cout << "Error creating directory " << name << std::endl;
					res = false;
				}
#endif
			}
		}

		return res;
	}

	struct PathStruct
	{
		std::string path;
		std::string name;
		std::string ext;

		bool isOk;

		PathStruct()
		{
			isOk = false;
		}
	};
	inline PathStruct ParsePathFileName(const std::string& vPathFileName)
	{
		PathStruct res;

		if (!vPathFileName.empty())
		{
			std::string pfn = vPathFileName;
			std::string separator(1u, PATH_SEP);
			replaceString(pfn, "\\", separator);
			replaceString(pfn, "/", separator);

			size_t lastSlash = pfn.find_last_of(separator);
			if (lastSlash != std::string::npos)
			{
				res.name = pfn.substr(lastSlash + 1);
				res.path = pfn.substr(0, lastSlash);
				res.isOk = true;
			}

			size_t lastPoint = pfn.find_last_of('.');
			if (lastPoint != std::string::npos)
			{
				if (!res.isOk)
				{
					res.name = pfn;
					res.isOk = true;
				}
				res.ext = pfn.substr(lastPoint + 1);
				replaceString(res.name, "." + res.ext, "");
			}
		}

		return res;
	}
	inline void AppendToBuffer(char* vBuffer, size_t vBufferLen, const std::string& vStr)
	{
		std::string st = vStr;
		size_t len = vBufferLen - 1u;
		size_t slen = strlen(vBuffer);

		if (!st.empty() && st != "\n")
		{
			replaceString(st, "\n", "");
			replaceString(st, "\r", "");
		}
		vBuffer[slen] = '\0';
		std::string str = std::string(vBuffer);
		//if (!str.empty()) str += "\n";
		str += vStr;
		if (len > str.size()) len = str.size();
#ifdef MSVC
		strncpy_s(vBuffer, vBufferLen, str.c_str(), len);
#else
		strncpy(vBuffer, str.c_str(), len);
#endif
		vBuffer[len] = '\0';
	}
	inline void ResetBuffer(char* vBuffer)
	{
		vBuffer[0] = '\0';
	}

	char ImGuiFileDialog::FileNameBuffer[MAX_FILE_DIALOG_NAME_BUFFER] = "";
	char ImGuiFileDialog::DirectoryNameBuffer[MAX_FILE_DIALOG_NAME_BUFFER] = "";
	char ImGuiFileDialog::SearchBuffer[MAX_FILE_DIALOG_NAME_BUFFER] = "";
	char ImGuiFileDialog::BookmarkEditBuffer[MAX_FILE_DIALOG_NAME_BUFFER] = "";
	
	ImGuiFileDialog::ImGuiFileDialog()
	{
		m_AnyWindowsHovered = false;
		IsOk = false;
		m_ShowDialog = false;
		m_ShowDrives = false;
		m_CreateDirectoryMode = false;
		dlg_optionsPane = nullptr;
		dlg_optionsPaneWidth = 250;
		dlg_filters = "";
        dlg_userDatas = 0;
		m_BookmarkPaneShown = false;
	}
	ImGuiFileDialog::~ImGuiFileDialog() = default;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	///// STANDARD DIALOG ////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	void ImGuiFileDialog::OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
		const std::string& vPath, const std::string& vDefaultFileName,
		const std::function<void(std::string, UserData, bool*)>& vOptionsPane, const size_t& vOptionsPaneWidth,
		const int& vCountSelectionMax, UserData vUserDatas)
	{
		if (m_ShowDialog) // if already opened, quit
			return;

		dlg_key = vKey;
		dlg_name = std::string(vName);
		dlg_filters = vFilters;
		ParseFilters(dlg_filters);
		dlg_path = vPath;
		dlg_defaultFileName = vDefaultFileName;
		dlg_optionsPane = std::move(vOptionsPane);
		dlg_userDatas = vUserDatas;
		dlg_optionsPaneWidth = vOptionsPaneWidth;
		dlg_countSelectionMax = vCountSelectionMax;
		dlg_modal = false;

		dlg_defaultExt = "";

		m_IsPathInputMode = false;
		m_SelectedFileItem = -1;

		SetPath(m_CurrentPath);

		m_ShowDialog = true;
		ResetBuffer(FileNameBuffer);
		ResetBuffer(SearchBuffer);
	}
	void ImGuiFileDialog::OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
		const std::string& vFilePathName,
		const std::function<void(std::string, UserData, bool*)>& vOptionsPane, const size_t& vOptionsPaneWidth,
		const int& vCountSelectionMax, UserData vUserDatas)
	{
		if (m_ShowDialog) // if already opened, quit
			return;

		dlg_key = vKey;
		dlg_name = std::string(vName);
		dlg_filters = vFilters;
		ParseFilters(dlg_filters);

		auto ps = ParsePathFileName(vFilePathName);
		if (ps.isOk)
		{
			dlg_path = ps.path;
			dlg_defaultFileName = vFilePathName;
			dlg_defaultExt = "." + ps.ext;
		}
		else
		{
			dlg_path = ".";
			dlg_defaultFileName = "";
			dlg_defaultExt = "";
		}

		dlg_optionsPane = std::move(vOptionsPane);
		dlg_userDatas = vUserDatas;
		dlg_optionsPaneWidth = vOptionsPaneWidth;
		dlg_countSelectionMax = vCountSelectionMax;
		dlg_modal = false;

		m_IsPathInputMode = false;
		m_SelectedFileItem = -1;

		SetSelectedFilterWithExt(dlg_defaultExt);
		SetPath(m_CurrentPath);

		m_ShowDialog = true;
		ResetBuffer(FileNameBuffer);
		ResetBuffer(SearchBuffer);
	}
	void ImGuiFileDialog::OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
		const std::string& vFilePathName, const int& vCountSelectionMax, UserData vUserDatas)
	{
		if (m_ShowDialog) // if already opened, quit
			return;

		dlg_key = vKey;
		dlg_name = std::string(vName);
		dlg_filters = vFilters;
		ParseFilters(dlg_filters);

		auto ps = ParsePathFileName(vFilePathName);
		if (ps.isOk)
		{
			dlg_path = ps.path;
			dlg_defaultFileName = vFilePathName;
			dlg_defaultExt = "." + ps.ext;
		}
		else
		{
			dlg_path = ".";
			dlg_defaultFileName = "";
			dlg_defaultExt = "";
		}

		dlg_optionsPane = nullptr;
		dlg_userDatas = vUserDatas;
		dlg_optionsPaneWidth = 0;
		dlg_countSelectionMax = vCountSelectionMax;
		dlg_modal = false;

		m_IsPathInputMode = false;
		m_SelectedFileItem = -1;

		SetSelectedFilterWithExt(dlg_defaultExt);
		SetPath(m_CurrentPath);

		m_ShowDialog = true;
		ResetBuffer(FileNameBuffer);
		ResetBuffer(SearchBuffer);
	}
	void ImGuiFileDialog::OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
		const std::string& vPath, const std::string& vDefaultFileName, const int& vCountSelectionMax, UserData vUserDatas)
	{
		if (m_ShowDialog) // if already opened, quit
			return;

		dlg_key = vKey;
		dlg_name = std::string(vName);
		dlg_filters = vFilters;
		ParseFilters(dlg_filters);
		dlg_path = vPath;
		dlg_defaultFileName = vDefaultFileName;
		dlg_optionsPane = nullptr;
		dlg_userDatas = vUserDatas;
		dlg_optionsPaneWidth = 0;
		dlg_countSelectionMax = vCountSelectionMax;
		dlg_modal = false;

		dlg_defaultExt = "";

		m_IsPathInputMode = false;
		m_SelectedFileItem = -1;

		SetPath(m_CurrentPath);

		m_ShowDialog = true;
		ResetBuffer(FileNameBuffer);
		ResetBuffer(SearchBuffer);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	///// STANDARD DIALOG ////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	void ImGuiFileDialog::OpenModal(const std::string& vKey, const char* vName, const char* vFilters,
		const std::string& vPath, const std::string& vDefaultFileName,
		const std::function<void(std::string, UserData, bool*)>& vOptionsPane, const size_t& vOptionsPaneWidth,
		const int& vCountSelectionMax, UserData vUserDatas)
	{
		if (m_ShowDialog) // if already opened, quit
			return;

		OpenDialog(vKey, vName, vFilters,
			vPath, vDefaultFileName,
			vOptionsPane, vOptionsPaneWidth,
			vCountSelectionMax, vUserDatas);
		dlg_modal = true;
	}
	void ImGuiFileDialog::OpenModal(const std::string& vKey, const char* vName, const char* vFilters,
		const std::string& vFilePathName,
		const std::function<void(std::string, UserData, bool*)>& vOptionsPane, const size_t& vOptionsPaneWidth,
		const int& vCountSelectionMax, UserData vUserDatas)
	{
		if (m_ShowDialog) // if already opened, quit
			return;

		OpenDialog(vKey, vName, vFilters,
			vFilePathName,
			vOptionsPane, vOptionsPaneWidth,
			vCountSelectionMax, vUserDatas);
		dlg_modal = true;
	}
	void ImGuiFileDialog::OpenModal(const std::string& vKey, const char* vName, const char* vFilters,
		const std::string& vFilePathName, const int& vCountSelectionMax, UserData vUserDatas)
	{
		if (m_ShowDialog) // if already opened, quit
			return;

		OpenDialog(vKey, vName, vFilters,
			vFilePathName, vCountSelectionMax, vUserDatas);
		dlg_modal = true;
	}
	void ImGuiFileDialog::OpenModal(const std::string& vKey, const char* vName, const char* vFilters,
		const std::string& vPath, const std::string& vDefaultFileName, const int& vCountSelectionMax, UserData vUserDatas)
	{
		if (m_ShowDialog) // if already opened, quit
			return;

		OpenDialog(vKey, vName, vFilters,
			vPath, vDefaultFileName, vCountSelectionMax, vUserDatas);
		dlg_modal = true;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	///// MAIN FUNCTION //////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	bool ImGuiFileDialog::FileDialog(const std::string& vKey, ImGuiWindowFlags vFlags, ImVec2 vMinSize, ImVec2 vMaxSize)
	{
		if (m_ShowDialog && dlg_key == vKey)
		{
			bool res = false;

			std::string name = dlg_name + "###" + dlg_key;

			IsOk = false;

			ImGui::SetNextWindowSizeConstraints(vMinSize, vMaxSize);
			ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

			bool beg = false;
			if (dlg_modal)
			{
				ImGui::OpenPopup(name.c_str());
				beg = ImGui::BeginPopupModal(name.c_str(), (bool*)nullptr, vFlags | ImGuiWindowFlags_NoScrollbar);
			}
			else
				beg = ImGui::Begin(name.c_str(), (bool*)nullptr, vFlags | ImGuiWindowFlags_NoScrollbar);

			if (beg) {
				m_Name = name;
				m_AnyWindowsHovered |= ImGui::IsWindowHovered();

				if (dlg_path.empty()) dlg_path = ".";

				if (m_SelectedFilter.empty())
				{
					if (!m_Filters.empty())
					{
						m_SelectedFilter = *m_Filters.begin();
					}
				}

				if (m_FileList.empty() && !m_ShowDrives)
				{
					replaceString(dlg_defaultFileName, dlg_path, ""); // local path

					if (!dlg_defaultFileName.empty())
					{
						ResetBuffer(FileNameBuffer);
						AppendToBuffer(FileNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER, dlg_defaultFileName);
						SetSelectedFilterWithExt(dlg_defaultExt);
					}
					
					ScanDir(dlg_path);
				}


				
				bool drivesClick = false;
#ifdef WIN32
				if (ImGui::Button("Drives"))
					drivesClick = true;
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("List all drives");
				ImGui::SameLine();
#endif

				if (ImGui::Button("+##CreateDirectory")) {
					ImGui::OpenPopup("create_dir_popup");
					ResetBuffer(DirectoryNameBuffer);
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Create a directory");
				if (ImGui::BeginPopup("create_dir_popup")) {
					ImGui::Text("Name:");
					ImGui::SameLine();

					ImGui::PushItemWidth(170.0f);
					ImGui::InputText("##DirectoryFileName", DirectoryNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER);
					ImGui::PopItemWidth();

					if (ImGui::Button("OK##ConfirmCreateDir")) {
						std::string newDir = std::string(DirectoryNameBuffer);
						if (CreateDir(newDir))
							SetPath(m_CurrentPath + PATH_SEP + newDir);

						ImGui::CloseCurrentPopup();
					}

					ImGui::SameLine();

					if (ImGui::Button("Cancel##CancelCreateDir"))
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}
				ImGui::SameLine();
				ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
				ImGui::SameLine();

				// show current path
				bool pathClick = false;
				if (m_IsPathInputMode) {
					ImGui::PushItemWidth(-1);
					if (ImGui::InputText("##DirectoryPathTB", m_PathInput, MAX_FILE_DIALOG_NAME_BUFFER, ImGuiInputTextFlags_EnterReturnsTrue)) {
						if (IsDirectoryExist(m_PathInput))
							SetPath(m_PathInput);
						m_IsPathInputMode = false;
					}
					if (ImGui::IsMouseClicked(0) && !ImGui::IsItemClicked())
						m_IsPathInputMode = false;
					ImGui::PopItemWidth();
				} else {
					if (!m_CurrentPath_Decomposition.empty()) {
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
						ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
						int pathElementIndex = 0;
						for (auto itPathDecomp = m_CurrentPath_Decomposition.begin();
							itPathDecomp != m_CurrentPath_Decomposition.end(); ++itPathDecomp)
						{
							if (itPathDecomp != m_CurrentPath_Decomposition.begin())
								ImGui::SameLine();
							if (ImGui::Button((*itPathDecomp + "##pelem" + std::to_string(pathElementIndex)).c_str()))
							{
								ComposeNewPath(itPathDecomp);
								pathClick = true;
								break;
							}

							if (pathElementIndex == m_CurrentPath_Decomposition.size() - 1 && *itPathDecomp != "/") {
								ImGui::SameLine();
								if (ImGui::Button("/##DirectoryInputState")) {
									strcpy(m_PathInput, m_CurrentPath.c_str());
									m_IsPathInputMode = true;
								}
							} else {
	#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
								if (*itPathDecomp != "/") {
									ImGui::SameLine();
									ImGui::Text("/");
								}
	#else
								ImGui::SameLine();
								ImGui::Text("/");
	#endif
							}

							pathElementIndex++;
						}
						ImGui::PopStyleVar();
						ImGui::PopStyleColor();
					}
					else 
						ImGui::Text("Drives");
				}


				// BOOKMARKS
				ToggleButton("Bookmarks", &m_BookmarkPaneShown);

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Toggle bookmarks pane");

				ImGui::SameLine();
				ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
				ImGui::SameLine();

				// SEARCH FIELD
				ImGui::Text("Search: ");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				if (ImGui::InputText("##ImGuiFileDialogSearchFiled", SearchBuffer, MAX_FILE_DIALOG_NAME_BUFFER))
				{
					searchTag = SearchBuffer;
                    ApplyFilteringOnFileList();
				}
				ImGui::PopItemWidth();

				



				static float lastBarHeight = 0.0f; // need one frame for calculate filelist size
				if (m_BookmarkPaneShown)
				{
					ImVec2 size = ImVec2(150.0f, ImGui::GetContentRegionAvail().y - lastBarHeight);
					DrawBookmarkPane(size);
					ImGui::SameLine();
				}

				ImVec2 size = ImGui::GetContentRegionAvail() - ImVec2((float)dlg_optionsPaneWidth, lastBarHeight);

				if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
					m_SelectedFileItem = -1;

				static ImGuiTableFlags flags = ImGuiTableFlags_SizingPolicyFixedX | 
					ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | 
					ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollFreezeTopRow | ImGuiTableFlags_Sortable;
				if (ImGui::BeginTable("##fileTable", 3, flags, size))
				{
					ImGui::TableSetupColumn(m_HeaderFileName.c_str(), ImGuiTableColumnFlags_WidthStretch, -1, 0);
					ImGui::TableSetupColumn(m_HeaderFileSize.c_str(), ImGuiTableColumnFlags_WidthAlwaysAutoResize, -1, 1);
					ImGui::TableSetupColumn(m_HeaderFileDate.c_str(), ImGuiTableColumnFlags_WidthAlwaysAutoResize, -1, 2);

					// Sort our data if sort specs have been changed!
					if (const ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
					{
						if (sorts_specs->SpecsChanged && !m_FileList.empty())
						{
							if (sorts_specs->Specs->ColumnUserID == 0)
								SortFields(SortingFieldEnum::FIELD_FILENAME, true);
							else if (sorts_specs->Specs->ColumnUserID == 1)
								SortFields(SortingFieldEnum::FIELD_SIZE, true);
							else if (sorts_specs->Specs->ColumnUserID == 2)
								SortFields(SortingFieldEnum::FIELD_DATE, true);
						}
					}

					ImGui::TableAutoHeaders();
	
					size_t countRows = m_FilteredFileList.size();
					ImGuiListClipper clipper(countRows, ImGui::GetTextLineHeightWithSpacing());
					for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
					{
						const FileInfoStruct& infos = m_FilteredFileList[i];

						ImVec4 c;
						std::string icon;
						bool showColor = GetExtentionInfos(infos.ext, &c, &icon);
						if (showColor)
							ImGui::PushStyleColor(ImGuiCol_Text, c);

						std::string str = " " + infos.fileName;
						if (infos.type == 'd') str = "[DIR]" + str;
						if (infos.type == 'l') str = "[LINK]" + str;
						if (infos.type == 'f')
						{
							if (showColor && !icon.empty())
								str = icon + str;
							else
								str = "[FILE]" + str;
						}
						bool selected = false;
						if (m_SelectedFileNames.find(infos.fileName) != m_SelectedFileNames.end()) // found
							selected = true;
						ImGui::TableNextRow();
						if (ImGui::TableSetColumnIndex(0)) // first column
						{
							ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns;
							if (ImGui::Selectable(str.c_str(), selected, selectableFlags))
							{
								if (infos.type == 'd')
								{
									if (ImGui::IsMouseDoubleClicked(0))
										pathClick = SelectDirectory(infos);
									else if (!dlg_filters)
										SelectFileName(infos);

									if (showColor)
										ImGui::PopStyleColor();

									break;
								} else {
									SelectFileName(infos);
									if (ImGui::IsMouseDoubleClicked(0)) {
										IsOk = true;
										res = true;
									}
								}
							}
							if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
								m_SelectedFileItem = i;
								m_SelectedFileItemPath = m_FilteredFileList[i].filePath + PATH_SEP + m_FilteredFileList[i].fileName;
							}
						}
						if (ImGui::TableSetColumnIndex(1)) // second column
						{
							if (infos.type != 'd')
								ImGui::Text("%s ", infos.formatedFileSize.c_str());
						}
						if (ImGui::TableSetColumnIndex(2)) // third column
							ImGui::Text("%s", infos.fileModifDate.c_str());

						if (showColor)
							ImGui::PopStyleColor();

					}
					clipper.End();
					ImGui::EndTable();
				}

				// table file dialog
				bool openAreYouSureDlg = false, openNewFileDlg = false;
				if (ImGui::BeginPopupContextItem("##ContextFileDialog")) {
					if (ImGui::Selectable("New file")) {
						openNewFileDlg = true;
						ResetBuffer(DirectoryNameBuffer); // was too lazy to add new variable
					}
					if (m_SelectedFileItem != -1 && ImGui::Selectable("Delete"))
						openAreYouSureDlg = true;
					ImGui::EndPopup();
				}
				if (openAreYouSureDlg)
					ImGui::OpenPopup("Are you sure?##delete");
				if (openNewFileDlg)
					ImGui::OpenPopup("Enter filename##newfile");
				if (ImGui::BeginPopupModal("Are you sure?##delete")) {
					ImGui::Text("Do you actually want to delete %s?", m_SelectedFileItemPath.c_str());
					if (ImGui::Button("Yes")) {
						std::error_code ec;
						std::filesystem::remove_all(std::filesystem::path(m_SelectedFileItemPath), ec);

						SetPath(m_CurrentPath);

						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if (ImGui::Button("No")) {
						m_SelectedFileItemPath = "dummy.txt";
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}
				if (ImGui::BeginPopupModal("Enter filename##newfile")) {
					ImGui::PushItemWidth(250.0f);
					ImGui::InputText("##NewFileName", DirectoryNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER);
					ImGui::PopItemWidth();

					if (ImGui::Button("OK")) {
						std::string newFile = std::string(GetCurrentPath() + PATH_SEP + DirectoryNameBuffer);
						std::ofstream out(newFile);
						out << "";
						out.close();

						SetPath(m_CurrentPath);

						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if (ImGui::Button("Cancel"))
						ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
				}

				// changement de repertoire
				if (pathClick)
					SetPath(m_CurrentPath);

				if (drivesClick)
					GetDrives();


				float posY = ImGui::GetCursorPos().y; // height of last bar calc
				bool _CanWeContinue = true;
				if (dlg_optionsPane)
				{
					ImGui::SameLine();
					size.x = (float)dlg_optionsPaneWidth;

					ImGui::BeginChild("##FileTypes", size);
					dlg_optionsPane(m_SelectedFilter.filter, dlg_userDatas, &_CanWeContinue);
					ImGui::EndChild();
				}

				if (dlg_filters) {
					ImGui::Text("File name: ");
					ImGui::SameLine();
					float width = ImGui::GetContentRegionAvail().x;
					if (dlg_filters)
						width -= 200.0f;
					ImGui::PushItemWidth(width);
					ImGui::InputText("##FileName", FileNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER);
					ImGui::PopItemWidth();

					ImGui::SameLine();

					bool needToApllyNewFilter = false;
					ImGui::PushItemWidth(-1);
					if (ImGui::BeginCombo("##Filters", m_SelectedFilter.filter.c_str(), ImGuiComboFlags_None))
					{
						intptr_t i = 0;
						for (auto filter : m_Filters) {
							const bool item_selected = (filter.filter == m_SelectedFilter.filter);
							ImGui::PushID((void*)(intptr_t)i++);
							if (ImGui::Selectable(filter.filter.c_str(), item_selected)) {
								m_SelectedFilter = filter;
								needToApllyNewFilter = true;
							}
							ImGui::PopID();
						}
						
						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();

					if (needToApllyNewFilter)
						SetPath(m_CurrentPath);
				}

				if (_CanWeContinue) {
					if (dlg_filters && FileNameBuffer[0] == 0) {
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}
					if (ImGui::Button("OK##DialogConfirm")) {
						if (FileNameBuffer[0] != '\0' || dlg_filters == nullptr)
						{
							IsOk = true;
							res = true;
						}
					}
					if (dlg_filters && FileNameBuffer[0] == 0) {
						ImGui::PopItemFlag();
						ImGui::PopStyleVar();
					}

					ImGui::SameLine();
				}

				if (ImGui::Button("Cancel##DialogCancel")) {
					IsOk = false;
					res = true;
				}

				lastBarHeight = ImGui::GetCursorPosY() - posY;

				if (dlg_modal)
					ImGui::EndPopup();
			}

			if (!dlg_modal)
				ImGui::End();

			return res;
		}

		return false;
	}

	void ImGuiFileDialog::CloseDialog(const std::string& vKey)
	{
		if (dlg_key == vKey) {
			dlg_key.clear();
			m_ShowDialog = false;
		}
	}

	std::string ImGuiFileDialog::GetFilepathName()
	{
		if (dlg_filters == nullptr)
			return this->GetCurrentPath();

		std::string result = m_CurrentPath;

		if (s_fs_root != result)
			result += PATH_SEP;
#ifdef _WIN32
		else
			result += PATH_SEP;
#endif

		result += GetCurrentFileName();

		return result;
	}

	std::string ImGuiFileDialog::GetCurrentPath()
	{
		return m_CurrentPath;
	}

	std::string ImGuiFileDialog::GetCurrentFileName()
	{
		std::string result = FileNameBuffer;

		// if not a collection we can replace the filter by thee extention we want
		if (!m_SelectedFilter.collectionfilters.empty() && m_SelectedFilter.filter != ".*") {
			size_t lastPoint = result.find_last_of('.');
			if (lastPoint == std::string::npos)
				result += *m_SelectedFilter.collectionfilters.begin();
		}

		return result;
	}

	std::string ImGuiFileDialog::GetCurrentFilter()
	{
		return m_SelectedFilter.filter;
	}

	UserData ImGuiFileDialog::GetUserDatas()
	{
		return dlg_userDatas;
	}

	std::map<std::string, std::string> ImGuiFileDialog::GetSelection()
	{
		std::map<std::string, std::string> res;

		for (auto & it : m_SelectedFileNames)
		{
			std::string  result = m_CurrentPath;

			if (s_fs_root != result)
				result += PATH_SEP;

			result += it;

			res[it] = result;
		}

		return res;
	}

	void ImGuiFileDialog::SetExtentionInfos(const std::string& vFilter, FileExtentionInfosStruct vInfos)
	{
		m_FileExtentionInfos[vFilter] = vInfos;
	}

	void ImGuiFileDialog::SetExtentionInfos(const std::string& vFilter, ImVec4 vColor, std::string vIcon)
	{
		m_FileExtentionInfos[vFilter] = FileExtentionInfosStruct(vColor, vIcon);
	}

	bool ImGuiFileDialog::GetExtentionInfos(const std::string& vFilter, ImVec4 *vColor, std::string *vIcon)
	{
		if (vColor)
		{
			if (m_FileExtentionInfos.find(vFilter) != m_FileExtentionInfos.end()) // found
			{
				*vColor = m_FileExtentionInfos[vFilter].color;
				if (vIcon)
				{
					*vIcon = m_FileExtentionInfos[vFilter].icon;
				}
				return true;
			}
		}
		return false;
	}

	void ImGuiFileDialog::ClearExtentionInfos()
	{
		m_FileExtentionInfos.clear();
	}

	bool ImGuiFileDialog::SelectDirectory(const FileInfoStruct& vInfos)
	{
		bool pathClick = false;

		if (vInfos.fileName == "..")
		{
			if (m_CurrentPath_Decomposition.size() > 1)
			{
				ComposeNewPath(m_CurrentPath_Decomposition.end() - 2);
				pathClick = true;
			}
		}
		else
		{
			std::string newPath;

			if (m_ShowDrives)
			{
				newPath = vInfos.fileName + PATH_SEP;
			}
			else
			{
#if defined(__linux__) || defined(__unix__)
				if (s_fs_root == m_CurrentPath)
				{
					newPath = m_CurrentPath + vInfos.fileName;
				}
				else
				{
#endif
					newPath = m_CurrentPath + PATH_SEP + vInfos.fileName;
#if defined(__linux__) || defined(__unix__)
				}
#endif
			}

			if (IsDirectoryExist(newPath))
			{
				if (m_ShowDrives)
				{
					m_CurrentPath = vInfos.fileName;
					s_fs_root = m_CurrentPath;
				}
				else
					m_CurrentPath = newPath;
				pathClick = true;
			}
		}

		return pathClick;
	}

	void ImGuiFileDialog::SelectFileName(const FileInfoStruct& vInfos)
	{
		if (ImGui::GetIO().KeyCtrl) {
			if (dlg_countSelectionMax == 0) // infinite selection
			{
				if (m_SelectedFileNames.find(vInfos.fileName) == m_SelectedFileNames.end()) // not found +> add
					AddFileNameInSelection(vInfos.fileName, true);
				else // found +> remove
					RemoveFileNameInSelection(vInfos.fileName);
			} else {
				if (m_SelectedFileNames.size() < dlg_countSelectionMax) {
					if (m_SelectedFileNames.find(vInfos.fileName) == m_SelectedFileNames.end()) // not found +> add
						AddFileNameInSelection(vInfos.fileName, true);
					else // found +> remove
						RemoveFileNameInSelection(vInfos.fileName);
				}
			}
		} else if (ImGui::GetIO().KeyShift) {
			if (dlg_countSelectionMax != 1) {
				m_SelectedFileNames.clear();
				// we will iterate filelist and get the last selection after the start selection
				bool startMultiSelection = false;
				std::string fileNameToSelect = vInfos.fileName;
				std::string savedLastSelectedFileName; // for invert selection mode
				for (auto & it : m_FileList)
				{
					const FileInfoStruct& infos = it;

					bool canTake = true;
					if (!searchTag.empty() && infos.fileName.find(searchTag) == std::string::npos) canTake = false;
					if (canTake) // if not filtered, we will take files who are filtered by the dialog
					{
						if (infos.fileName == m_LastSelectedFileName)
						{
							startMultiSelection = true;
							AddFileNameInSelection(m_LastSelectedFileName, false);
						}
						else if (startMultiSelection)
						{
							if (dlg_countSelectionMax == 0) // infinite selection
								AddFileNameInSelection(infos.fileName, false);
							else // selection limited by size
							{
								if (m_SelectedFileNames.size() < dlg_countSelectionMax)
									AddFileNameInSelection(infos.fileName, false);
								else
								{
									startMultiSelection = false;
									if (!savedLastSelectedFileName.empty())
										m_LastSelectedFileName = savedLastSelectedFileName;
									break;
								}
							}
						}

						if (infos.fileName == fileNameToSelect)
						{
							if (!startMultiSelection) // we are before the last Selected FileName, so we must inverse
							{
								savedLastSelectedFileName = m_LastSelectedFileName;
								m_LastSelectedFileName = fileNameToSelect;
								fileNameToSelect = savedLastSelectedFileName;
								startMultiSelection = true;
								AddFileNameInSelection(m_LastSelectedFileName, false);
							}
							else
							{
								startMultiSelection = false;
								if (!savedLastSelectedFileName.empty())
									m_LastSelectedFileName = savedLastSelectedFileName;
								break;
							}
						}
					}
				}
			}
		} else {
			m_SelectedFileNames.clear();
			ResetBuffer(FileNameBuffer);
			AddFileNameInSelection(vInfos.fileName, true);
		}
	}

	void ImGuiFileDialog::RemoveFileNameInSelection(const std::string& vFileName)
	{
		m_SelectedFileNames.erase(vFileName);

		if (m_SelectedFileNames.size() == 1)
			snprintf(FileNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER, "%s", vFileName.c_str());
		else
			snprintf(FileNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER, "%zu files Selected", m_SelectedFileNames.size());
	}

	void ImGuiFileDialog::AddFileNameInSelection(const std::string& vFileName, bool vSetLastSelectionFileName)
	{
		m_SelectedFileNames.emplace(vFileName);

		if (m_SelectedFileNames.size() == 1)
			snprintf(FileNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER, "%s", vFileName.c_str());
		else
			snprintf(FileNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER, "%zu files Selected", m_SelectedFileNames.size());

		if (vSetLastSelectionFileName)
			m_LastSelectedFileName = vFileName;
	}

	void ImGuiFileDialog::SetPath(const std::string& vPath)
	{
		m_ShowDrives = false;
		m_CurrentPath = vPath;
		m_FileList.clear();
		m_CurrentPath_Decomposition.clear();
		ScanDir(m_CurrentPath);
	}

	static std::string round_n(double vvalue, int n)
	{
		std::stringstream tmp;
		tmp << std::setprecision(n) << std::fixed << vvalue;
		return tmp.str();
	}

	static void FormatFileSize(size_t vByteSize, std::string *vFormat)
	{
		if (vFormat && vByteSize != 0)
		{
			static double lo = 1024;
			static double ko = 1024 * 1024;
			static double mo = 1024 * 1024 * 1024;

			double v = (double)vByteSize;

			if (vByteSize < lo) 
				*vFormat = round_n(v, 0) + " b"; // octet
			else if (vByteSize < ko) 
				*vFormat = round_n(v / lo, 2) + " Kb"; // ko
			else  if (vByteSize < mo) 
				*vFormat = round_n(v / ko, 2) + " Mb"; // Mo 
			else 
				*vFormat = round_n(v / mo, 2) + " Gb"; // Go 
		}
	}

	void ImGuiFileDialog::FillInfos(FileInfoStruct *vFileInfoStruct)
	{
		if (vFileInfoStruct && vFileInfoStruct->fileName != "..")
		{
			// _stat struct :
			//dev_t     st_dev;     /* ID of device containing file */
			//ino_t     st_ino;     /* inode number */
			//mode_t    st_mode;    /* protection */
			//nlink_t   st_nlink;   /* number of hard links */
			//uid_t     st_uid;     /* user ID of owner */
			//gid_t     st_gid;     /* group ID of owner */
			//dev_t     st_rdev;    /* device ID (if special file) */
			//off_t     st_size;    /* total size, in bytes */
			//blksize_t st_blksize; /* blocksize for file system I/O */
			//blkcnt_t  st_blocks;  /* number of 512B blocks allocated */
			//time_t    st_atime;   /* time of last access - not sure out of ntfs */
			//time_t    st_mtime;   /* time of last modification - not sure out of ntfs */
			//time_t    st_ctime;   /* time of last status change - not sure out of ntfs */

			std::string fpn;

			if (vFileInfoStruct->type == 'f') // file
				fpn = vFileInfoStruct->filePath + PATH_SEP + vFileInfoStruct->fileName;
			else if (vFileInfoStruct->type == 'l') // link
				fpn = vFileInfoStruct->filePath + PATH_SEP + vFileInfoStruct->fileName;
			else if (vFileInfoStruct->type == 'd') // directory
				fpn = vFileInfoStruct->filePath + PATH_SEP + vFileInfoStruct->fileName;

			struct stat statInfos;
			char timebuf[100];
			int result = stat(fpn.c_str(), &statInfos);
			if (!result)
			{
				if (vFileInfoStruct->type != 'd')
				{
					vFileInfoStruct->fileSize = statInfos.st_size;
					FormatFileSize(vFileInfoStruct->fileSize, 
						&vFileInfoStruct->formatedFileSize);
				}

				size_t len = 0;
#ifdef MSVC
				struct tm _tm;
				errno_t err = localtime_s(&_tm, &statInfos.st_mtime);
				if (!err) len = strftime(timebuf, 99, "%Y/%m/%d ", &_tm);
#else
				struct tm *_tm = localtime(&statInfos.st_mtime);
				if (_tm) len = strftime(timebuf, 99, "%Y/%m/%d ", _tm);
#endif
				if (len)
				{
					vFileInfoStruct->fileModifDate = std::string(timebuf, len);
				}
			}
		}
	}

    void ImGuiFileDialog::SortFields(SortingFieldEnum vSortingField, bool vCanChangeOrder)
    {
		if (vSortingField != SortingFieldEnum::FIELD_NONE)
		{
			m_HeaderFileName = "Name";
			m_HeaderFileSize = "Size";
			m_HeaderFileDate = "Date";
		}

		if (vSortingField == SortingFieldEnum::FIELD_FILENAME)
		{
			if (vCanChangeOrder && m_SortingField == vSortingField)
				m_SortingDirection[0] = !m_SortingDirection[0];

			if (m_SortingDirection[0]) {
				std::sort(m_FileList.begin(), m_FileList.end(),
					[](const FileInfoStruct & a, const FileInfoStruct & b) -> bool
				{
					if (a.type != b.type) return (a.type == 'd'); // directory in first
					return (stricmp(a.fileName.c_str(), b.fileName.c_str()) < 0); // sort in insensitive case
				});
			} else {
				std::sort(m_FileList.begin(), m_FileList.end(),
					[](const FileInfoStruct & a, const FileInfoStruct & b) -> bool
				{
					if (a.type != b.type) return (a.type != 'd'); // directory in last
					return (stricmp(a.fileName.c_str(), b.fileName.c_str()) > 0); // sort in insensitive case
				});
			}
		}
		else if (vSortingField == SortingFieldEnum::FIELD_SIZE)
		{
			if (vCanChangeOrder && m_SortingField == vSortingField)
				m_SortingDirection[1] = !m_SortingDirection[1];

			if (m_SortingDirection[1]) {
				std::sort(m_FileList.begin(), m_FileList.end(),
					[](const FileInfoStruct & a, const FileInfoStruct & b) -> bool
				{
					if (a.type != b.type) return (a.type == 'd'); // directory in first
					return (a.fileSize < b.fileSize); // else
				});
			} else {
				std::sort(m_FileList.begin(), m_FileList.end(),
					[](const FileInfoStruct & a, const FileInfoStruct & b) -> bool
				{
					if (a.type != b.type) return (a.type != 'd'); // directory in last
					return (a.fileSize > b.fileSize); // else
				});
			}
		}
		else if (vSortingField == SortingFieldEnum::FIELD_DATE)
		{
			if (vCanChangeOrder && m_SortingField == vSortingField)
				m_SortingDirection[2] = !m_SortingDirection[2];

			if (m_SortingDirection[2]) {
				std::sort(m_FileList.begin(), m_FileList.end(),
					[](const FileInfoStruct & a, const FileInfoStruct & b) -> bool
				{
					if (a.type != b.type) return (a.type == 'd'); // directory in first
					return (a.fileModifDate < b.fileModifDate); // else
				});
			} else {
				std::sort(m_FileList.begin(), m_FileList.end(),
					[](const FileInfoStruct & a, const FileInfoStruct & b) -> bool
				{
					if (a.type != b.type) return (a.type != 'd'); // directory in last
					return (a.fileModifDate > b.fileModifDate); // else
				});
			}
		}
		
		if (vSortingField != SortingFieldEnum::FIELD_NONE)
			m_SortingField = vSortingField;

		ApplyFilteringOnFileList();
    }

	void ImGuiFileDialog::ScanDir(const std::string& vPath)
	{
		struct dirent **files = nullptr;
		int             i = 0;
		int             n = 0;
		std::string		path = vPath;

		if (m_CurrentPath_Decomposition.empty())
			SetCurrentDir(path);

		if (!m_CurrentPath_Decomposition.empty())
		{
#ifdef WIN32
			if (path == s_fs_root)
				path += PATH_SEP;
#endif
			n = scandir(path.c_str(), &files, nullptr, alphaSort);

            m_FileList.clear();

            if (n > 0)
			{
				for (i = 0; i < n; i++)
				{
					struct dirent *ent = files[i];

					FileInfoStruct infos;

					infos.filePath = path;
					infos.fileName = ent->d_name;
					infos.fileName_optimized = OptimizeFilenameForSearchOperations(infos.fileName);

					if (("." != infos.fileName))
					{
						switch (ent->d_type)
						{
						case DT_REG: 
							infos.type = 'f'; break;
						case DT_DIR: 
							infos.type = 'd'; break;
						case DT_LNK: 
							infos.type = 'l'; break;
						}

						if (infos.type == 'f' || 
							infos.type == 'l') // link can have the same extention of a file
						{
							size_t lpt = infos.fileName.find_last_of('.');
							if (lpt != std::string::npos)
								infos.ext = infos.fileName.substr(lpt);

							if (dlg_filters)
							{
								// check if current file extention is covered by current filter
								// we do that here, for avoid doing taht during filelist display
								// for better fps
								if (!m_SelectedFilter.empty() && // selected filter exist
									(!m_SelectedFilter.filterExist(infos.ext) && // filter not found
										m_SelectedFilter.filter != ".*"))
								{
									continue;
								}
							}
						}

						FillInfos(&infos);
						m_FileList.push_back(infos);
					}
				}

				for (i = 0; i < n; i++)
					free(files[i]);
				free(files);
			}

			SortFields(m_SortingField);
		}
	}

	void ImGuiFileDialog::SetCurrentDir(const std::string& vPath)
	{
		std::string path = vPath;
#ifdef WIN32
		if (s_fs_root == path)
			path += PATH_SEP;
#endif
		DIR  *dir = opendir(path.c_str());
		char  real_path[PATH_MAX];

		if (nullptr == dir)
		{
			path = ".";
			dir = opendir(path.c_str());
		}

		if (nullptr != dir)
		{
#if defined(WIN32)
			size_t numchar = GetFullPathNameA(path.c_str(), PATH_MAX - 1, real_path, nullptr);
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
			char *numchar = realpath(path.c_str(), real_path);
#endif
			if (numchar != 0)
			{
				m_CurrentPath = real_path;
				if (m_CurrentPath[m_CurrentPath.size() - 1] == PATH_SEP)
				{
					m_CurrentPath = m_CurrentPath.substr(0, m_CurrentPath.size() - 1);
				}
				m_CurrentPath_Decomposition = splitStringToVector(m_CurrentPath, PATH_SEP, false);
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
				m_CurrentPath_Decomposition.insert(m_CurrentPath_Decomposition.begin(), std::string(1u, PATH_SEP));
#endif
				if (!m_CurrentPath_Decomposition.empty())
				{
#ifdef WIN32
					s_fs_root = m_CurrentPath_Decomposition[0];
#endif
				}
			}

			closedir(dir);
		}
	}

	bool ImGuiFileDialog::CreateDir(const std::string& vPath)
	{
		bool res = false;

		if (!vPath.empty())
		{
			std::string path = m_CurrentPath + PATH_SEP + vPath;
			res = CreateDirectoryIfNotExist(path);
		}

		return res;
	}

	void ImGuiFileDialog::ComposeNewPath(std::vector<std::string>::iterator vIter)
	{
		m_CurrentPath = "";

		while (true)
		{
			if (!m_CurrentPath.empty())
			{
#if defined(WIN32)
				m_CurrentPath = *vIter + PATH_SEP + m_CurrentPath;
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
				if (*vIter == s_fs_root)
					m_CurrentPath = *vIter + m_CurrentPath;
				else
					m_CurrentPath = *vIter + PATH_SEP + m_CurrentPath;
#endif
			}
			else
				m_CurrentPath = *vIter;

			if (vIter == m_CurrentPath_Decomposition.begin())
			{
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
				if (m_CurrentPath[0] != PATH_SEP)
					m_CurrentPath = PATH_SEP + m_CurrentPath;
#endif
				break;
			}

			vIter--;
		}
	}

	void ImGuiFileDialog::GetDrives()
	{
		auto res = GetDrivesList();
		if (!res.empty())
		{
			m_CurrentPath = "";
			m_CurrentPath_Decomposition.clear();
			m_FileList.clear();
			for (auto & re : res)
			{
				FileInfoStruct infos;
				infos.fileName = re;
				infos.fileName_optimized = OptimizeFilenameForSearchOperations(re);
				infos.type = 'd';

				if (!infos.fileName.empty())
				{
					m_FileList.push_back(infos);
				}
			}
			m_ShowDrives = true;
            ApplyFilteringOnFileList();
		}
	}

	void ImGuiFileDialog::ParseFilters(const char *vFilters)
	{
		m_Filters.clear();

		if (vFilters)
		{
			std::string fullStr = vFilters;

			// ".*,.cpp,.h,.hpp"
			// "Source files{.cpp,.h,.hpp},Image files{.png,.gif,.jpg,.jpeg},.md"

			bool currentFilterFound = false;

			size_t nan = std::string::npos;
			size_t p = 0, lp = 0;
			while ((p = fullStr.find_first_of("{,", p)) != nan)
			{
				FilterInfosStruct infos;

				if (fullStr[p] == '{') // {
				{
					infos.filter = fullStr.substr(lp, p - lp);
					p++;
					lp = fullStr.find('}', p);
					if (lp != nan)
					{
						std::string fs = fullStr.substr(p, lp - p);
						auto arr = splitStringToVector(fs, ',', false);
						for (auto a : arr)
						{
							infos.collectionfilters.emplace(a);
						}
					}
					p = lp + 1;
				}
				else // ,
				{
					infos.filter = fullStr.substr(lp, p - lp);
					p++;
				}

				if (!currentFilterFound && m_SelectedFilter.filter == infos.filter)
				{
					currentFilterFound = true;
					m_SelectedFilter = infos;
				}

				lp = p;
				if (!infos.empty())
					m_Filters.emplace_back(infos);
			}

			std::string token = fullStr.substr(lp);
			if (!token.empty())
			{
				FilterInfosStruct infos;
				infos.filter = token;
				m_Filters.emplace_back(infos);
			}

			if (!currentFilterFound)
				if (!m_Filters.empty())
					m_SelectedFilter = *m_Filters.begin();
		}
	}

	void ImGuiFileDialog::SetSelectedFilterWithExt(const std::string& vFilter)
	{
		if (!m_Filters.empty())
		{
			if (!vFilter.empty())
			{
				// std::map<std::string, FilterInfosStruct>
				for (auto infos : m_Filters)
				{
					if (vFilter == infos.filter)
					{
						m_SelectedFilter = infos;
					}
					else
					{
						// maybe this ext is in an extention so we will 
						// explore the collections is they are existing
						for (auto filter : infos.collectionfilters)
						{
							if (vFilter == filter)
							{
								m_SelectedFilter = infos;
							}
						}
					}
				}
			}

			if (m_SelectedFilter.empty())
				m_SelectedFilter = *m_Filters.begin();
		}
	}
	
	std::string ImGuiFileDialog::OptimizeFilenameForSearchOperations(std::string vFileName)
	{
		// convert to lower case
		for (char & c : vFileName)
			c = (char)std::tolower(c);
		return vFileName;
	}

    void ImGuiFileDialog::ApplyFilteringOnFileList()
    {
        m_FilteredFileList.clear();

        for (auto &it : m_FileList)
        {
            const FileInfoStruct &infos = it;

            bool show = true;

            // if search tag
            if (!searchTag.empty() &&
                infos.fileName_optimized.find(searchTag) == std::string::npos && // first try wihtout case and accents
                infos.fileName.find(searchTag) == std::string::npos) // second if searched with case and accents
            {
                show = false;
            }

            if (!dlg_filters && infos.type != 'd') // directory mode
            {
                show = false;
            }

            if (show)
            {
                m_FilteredFileList.push_back(infos);
            }
        }
    }

	void ImGuiFileDialog::DrawBookmarkPane(ImVec2 vSize)
	{
		ImGui::BeginChild("##bookmarkpane", vSize);
		if (ImGui::Button("+##ImGuiFileDialogAddBookmark"))
		{
			if (!m_CurrentPath_Decomposition.empty())
			{
				BookmarkStruct bookmark;
				bookmark.name = m_CurrentPath_Decomposition.back();
				bookmark.path = m_CurrentPath;
				m_Bookmarks.push_back(bookmark);
			}
		}
		static size_t selectedBookmarkForEdition = -1;
		bool disableControls = !(selectedBookmarkForEdition >= 0 && selectedBookmarkForEdition < m_Bookmarks.size());

		if (disableControls) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

			ImGui::SameLine();
			if (ImGui::Button("-##ImGuiFileDialogAddBookmark"))
			{
				m_Bookmarks.erase(m_Bookmarks.begin() + selectedBookmarkForEdition);
				if (selectedBookmarkForEdition == m_Bookmarks.size())
					selectedBookmarkForEdition--;
			}
			if (selectedBookmarkForEdition >= 0)
			{
				ImGui::PushItemWidth(150.0f);
				if (ImGui::InputText("##ImGuiFileDialogBookmarkEdit", BookmarkEditBuffer, MAX_FILE_DIALOG_NAME_BUFFER))
				{
					m_Bookmarks[selectedBookmarkForEdition].name = std::string(BookmarkEditBuffer);
				}
				ImGui::PopItemWidth();
			}

		if (disableControls) {
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}

		ImGui::Separator();
		size_t countRows = m_Bookmarks.size();
		ImGuiListClipper clipper(countRows, ImGui::GetTextLineHeightWithSpacing());
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
		{
			const BookmarkStruct& bookmark = m_Bookmarks[i];
			ImGui::PushID(i);
			if (ImGui::Selectable(bookmark.name.c_str(), selectedBookmarkForEdition == i, ImGuiSelectableFlags_AllowDoubleClick) ||
				(selectedBookmarkForEdition == -1 && bookmark.path == m_CurrentPath)) // select if path is current
			{
				selectedBookmarkForEdition = i;
				ResetBuffer(BookmarkEditBuffer);
				AppendToBuffer(BookmarkEditBuffer, MAX_FILE_DIALOG_NAME_BUFFER, bookmark.name);

				if (ImGui::IsMouseDoubleClicked(0)) // apply path
				{
					SetPath(bookmark.path);
				}
			}
			ImGui::PopID();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(bookmark.path.c_str());
		}
		clipper.End();
		ImGui::EndChild();
	}

	std::string ImGuiFileDialog::SerializeBookmarks()
	{
		std::string res;

		size_t idx = 0;
		for (auto & it : m_Bookmarks)
		{
			if (idx++ != 0)
				res += "##"; // ## because reserved by imgui, so an input text cant have ##
			res += it.name + "##" + it.path;
		}

		return res;
	}

	void ImGuiFileDialog::DeserializeBookmarks(std::string vBookmarks)
	{
		if (!vBookmarks.empty())
		{
			m_Bookmarks.clear();
			auto arr = splitStringToVector(vBookmarks, '#', false);
			for (size_t i = 0; i < arr.size(); i += 2)
			{
				BookmarkStruct bookmark;
				bookmark.name = arr[i];
				if (i + 1 < arr.size()) // for avoid crash if arr size is impair due to user mistake after edition
				{
					// if bad format we jump this bookmark
					bookmark.path = arr[i + 1];
					m_Bookmarks.push_back(bookmark);
				}
			}
		}
	}
}

