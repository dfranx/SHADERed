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

#pragma once

#define IMGUIFILEDIALOG_VERSION "v0.4"

#include <imgui/imgui.h>

#include <float.h>

#include <vector>
#include <string>
#include <set>
#include <map>
#include <unordered_map>

#include <functional>
#include <string>
#include <vector>
#include <list>

namespace igfd
{
	#define MAX_FILE_DIALOG_NAME_BUFFER 1024

	typedef void* UserData;

	struct FileInfoStruct
	{
		char type = ' ';
		std::string filePath;
		std::string fileName;
		std::string fileName_optimized; // optimized for search => insensitivecase
		std::string ext;
		size_t fileSize = 0; // for sorting operations
		std::string formatedFileSize;
		std::string fileModifDate;
	};

	// old FilterInfosStruct
	struct FileExtentionInfosStruct
	{
		std::string icon;
		ImVec4 color = ImVec4(0, 0, 0, 0);
		FileExtentionInfosStruct() { color = ImVec4(0, 0, 0, 0); }
		FileExtentionInfosStruct(const ImVec4& vColor, const std::string& vIcon = std::string())
		{
			color = vColor;
			icon = vIcon;
		}
	};

	struct FilterInfosStruct
	{
		std::string filter;
		std::set<std::string> collectionfilters;

		void clear()
		{
			filter.clear();
			collectionfilters.clear();
		}

		bool empty()
		{
			return 
				filter.empty() && 
				collectionfilters.empty();
		}

		bool filterExist(std::string vFilter)
		{
			return
				filter == vFilter ||
				collectionfilters.find(vFilter) != collectionfilters.end();
		}
	};

	enum SortingFieldEnum
    {
		FIELD_NONE = 0,
	    FIELD_FILENAME,
	    FIELD_SIZE,
		FIELD_DATE
    };

	struct BookmarkStruct
	{
		std::string name;
		std::string path;
	};

	class ImGuiFileDialog
	{
	private:
		std::vector<FileInfoStruct> m_FileList;
        std::vector<FileInfoStruct> m_FilteredFileList;
        std::unordered_map<std::string, FileExtentionInfosStruct> m_FileExtentionInfos;
		std::set<std::string> m_SelectedFileNames;
		std::string m_CurrentPath;
		std::vector<std::string> m_CurrentPath_Decomposition;
		std::string m_Name;
		bool m_ShowDialog = false;
		bool m_ShowDrives = false;
		std::string m_LastSelectedFileName; // for shift multi selectio
		std::vector<FilterInfosStruct> m_Filters;
		FilterInfosStruct m_SelectedFilter;
		std::vector<BookmarkStruct> m_Bookmarks;
		bool m_BookmarkPaneShown = false;
		int m_SelectedFileItem;
		std::string m_SelectedFileItemPath;

	private: // flash when select by char
		size_t m_FlashedItem = 0;
		float m_FlashAlpha = 0.0f;
		float m_FlashAlphaAttenInSecs = 1.0f; // fps display dependant

	public:
		static char FileNameBuffer[MAX_FILE_DIALOG_NAME_BUFFER];
		static char DirectoryNameBuffer[MAX_FILE_DIALOG_NAME_BUFFER];
		static char SearchBuffer[MAX_FILE_DIALOG_NAME_BUFFER];
		static char BookmarkEditBuffer[MAX_FILE_DIALOG_NAME_BUFFER];

		bool IsOk = false;
		bool m_AnyWindowsHovered = false;
		bool m_CreateDirectoryMode = false;

	private:
		std::string dlg_key;
		std::string dlg_name;
		const char *dlg_filters{};
		std::string dlg_path;
		std::string dlg_defaultFileName;
		std::string dlg_defaultExt;
		std::function<void(std::string, UserData, bool*)> dlg_optionsPane;
		size_t dlg_optionsPaneWidth = 0;
		std::string searchTag;
		UserData dlg_userDatas{};
		size_t dlg_countSelectionMax = 1; // 0 for infinite
		bool dlg_modal = false;
		
	private:
		bool m_IsPathInputMode;
		char m_PathInput[MAX_FILE_DIALOG_NAME_BUFFER];

		std::string m_HeaderFileName;
		std::string m_HeaderFileSize;
		std::string m_HeaderFileDate;
		bool m_SortingDirection[3] = { false,true,true }; // true => Descending, false => Ascending
		SortingFieldEnum m_SortingField = SortingFieldEnum::FIELD_FILENAME;

	public:
		static ImGuiFileDialog* Instance()
		{
			static ImGuiFileDialog* _instance = new ImGuiFileDialog();
			return _instance;
		}

	protected:
		ImGuiFileDialog(); // Prevent construction
		ImGuiFileDialog(const ImGuiFileDialog&) {}; // Prevent construction by copying
		ImGuiFileDialog& operator =(const ImGuiFileDialog&) { return *this; }; // Prevent assignment
		~ImGuiFileDialog(); // Prevent unwanted destruction

	public: // standard dialog
		void OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
			const std::string& vPath, const std::string& vDefaultFileName,
			const std::function<void(std::string, UserData, bool*)>& vOptionsPane, const size_t&  vOptionsPaneWidth = 250,
			const int& vCountSelectionMax = 1, UserData vUserDatas = 0);
		void OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
			const std::string& vDefaultFileName,
			const std::function<void(std::string, UserData, bool*)>& vOptionsPane, const size_t&  vOptionsPaneWidth = 250,
			const int& vCountSelectionMax = 1, UserData vUserDatas = 0);
		void OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
			const std::string& vPath, const std::string& vDefaultFileName,
			const int& vCountSelectionMax = 1, UserData vUserDatas = 0);
		void OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
			const std::string& vFilePathName, const int& vCountSelectionMax = 1,
			UserData vUserDatas = 0);

	public: // modal dialog
		void OpenModal(const std::string& vKey, const char* vName, const char* vFilters,
			const std::string& vPath, const std::string& vDefaultFileName,
			const std::function<void(std::string, UserData, bool*)>& vOptionsPane, const size_t&  vOptionsPaneWidth = 250,
			const int& vCountSelectionMax = 1, UserData vUserDatas = 0);
		void OpenModal(const std::string& vKey, const char* vName, const char* vFilters,
			const std::string& vDefaultFileName,
			const std::function<void(std::string, UserData, bool*)>& vOptionsPane, const size_t&  vOptionsPaneWidth = 250,
			const int& vCountSelectionMax = 1, UserData vUserDatas = 0);
		void OpenModal(const std::string& vKey, const char* vName, const char* vFilters,
			const std::string& vPath, const std::string& vDefaultFileName,
			const int& vCountSelectionMax = 1, UserData vUserDatas = 0);
		void OpenModal(const std::string& vKey, const char* vName, const char* vFilters,
			const std::string& vFilePathName, const int& vCountSelectionMax = 1,
			UserData vUserDatas = 0);

	public: // core
		bool FileDialog(const std::string& vKey, ImGuiWindowFlags vFlags = ImGuiWindowFlags_NoCollapse,
			ImVec2 vMinSize = ImVec2(0, 0), ImVec2 vMaxSize = ImVec2(FLT_MAX, FLT_MAX));
		void CloseDialog(const std::string& vKey);

		std::string GetFilepathName();
		std::string GetCurrentPath();
		std::string GetCurrentFileName();
		std::string GetCurrentFilter();
		UserData GetUserDatas();
		std::map<std::string, std::string> GetSelection(); // return map<FileName, FilePathName>

		void SetExtentionInfos(const std::string& vFilter, FileExtentionInfosStruct vInfos);
		void SetExtentionInfos(const std::string& vFilter, ImVec4 vColor, std::string vIcon = "");
		bool GetExtentionInfos(const std::string& vFilter, ImVec4 *vColor, std::string *vIcon = 0);
		void ClearExtentionInfos();

	private:
		bool SelectDirectory(const FileInfoStruct& vInfos);
		void SelectFileName(const FileInfoStruct& vInfos);
		void RemoveFileNameInSelection(const std::string& vFileName);
		void AddFileNameInSelection(const std::string& vFileName, bool vSetLastSelectionFileName);
		void SetPath(const std::string& vPath);
		void FillInfos(FileInfoStruct *vFileInfoStruct);
		void SortFields(SortingFieldEnum vSortingField = SortingFieldEnum::FIELD_NONE, bool vCanChangeOrder = false);
		void ScanDir(const std::string& vPath);
		void SetCurrentDir(const std::string& vPath);
		bool CreateDir(const std::string& vPath);
		void ComposeNewPath(std::vector<std::string>::iterator vIter);
		void GetDrives();
		void ParseFilters(const char *vFilters);
		void SetSelectedFilterWithExt(const std::string& vFilter);
		std::string OptimizeFilenameForSearchOperations(std::string vFileName);

	private:
	    void ApplyFilteringOnFileList();

	public:
		void DrawBookmarkPane(ImVec2 vSize);
		std::string SerializeBookmarks();
		void DeserializeBookmarks(std::string vBookmarks);
	};
}
