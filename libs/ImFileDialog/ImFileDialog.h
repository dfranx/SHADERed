#pragma once
#include <ctime>
#include <stack>
#include <string>
#include <thread>
#include <functional>
#include <unordered_map>

#define IFD_DIALOG_FILE			0
#define IFD_DIALOG_DIRECTORY	1
#define IFD_DIALOG_SAVE			2

namespace ifd {
	class FileDialog {
	public:
		static inline FileDialog& Instance()
		{
			static FileDialog ret;
			return ret;
		}

		FileDialog();
		~FileDialog();

		bool Save(const std::string& key, const std::string& title, const std::string& filter, const std::string& startingDir = "");

		bool Open(const std::string& key, const std::string& title, const std::string& filter, const std::string& startingDir = "");

		bool IsDone(const std::string& key);

		inline bool HasResult() { return m_hasResult; }
		inline const std::wstring& GetResult() { return m_result; }

		void Close();

		void RemoveFavorite(const std::wstring& path);
		void AddFavorite(const std::wstring& path);
		inline const std::vector<std::wstring>& GetFavorites() { return m_favorites; }

		inline void SetZoom(float z) { 
			m_zoom = std::min<float>(25.0f, std::max<float>(1.0f, z)); 
			m_refreshIconPreview();
		}
		inline float GetZoom() { return m_zoom; }

		std::function<void*(uint8_t*, int, int, char)> CreateTexture; // char -> fmt -> { 0 = BGRA, 1 = RGBA }
		std::function<void(void*)> DeleteTexture;

		class FileTreeNode {
		public:
			FileTreeNode(const std::wstring& path) {
				Path = path;
				Read = false;
			}

			std::wstring Path;
			bool Read;
			std::vector<FileTreeNode*> Children;
		};
		class FileData {
		public:
			FileData(const std::wstring& path);

			std::wstring Path;
			bool IsDirectory;
			size_t Size;
			time_t DateModified;

			bool HasIconPreview;
			void* IconPreview;
			uint8_t* IconPreviewData;
			int IconPreviewWidth, IconPreviewHeight;
		};

	private:
		std::string m_currentKey;
		std::string m_currentTitle;
		std::wstring m_currentDirectory;
		bool m_isOpen;
		uint8_t m_type;
		char m_inputTextbox[1024];
		char m_pathBuffer[1024];
		char m_newEntryBuffer[1024];
		char m_searchBuffer[128];
		std::vector<std::wstring> m_favorites;
		bool m_calledOpenPopup;
		std::stack<std::wstring> m_backHistory, m_forwardHistory;
		int m_selectedFileItem;

		float m_zoom;

		std::wstring m_result;
		bool m_hasResult;
		bool m_finalize(const std::wstring& filename = L"");

		std::string m_filter;
		std::vector<std::vector<std::string>> m_filterExtensions;
		int m_filterSelection;
		void m_parseFilter(const std::string& filter);

		std::vector<int> m_iconIndices;
		std::vector<std::wstring> m_iconFilepaths; // m_iconIndices[x] <-> m_iconFilepaths[x]
		std::unordered_map<std::wstring, void*> m_icons;
		void* m_getIcon(const std::wstring& path);
		void m_clearIcons();
		void m_refreshIconPreview();
		void m_clearIconPreview();

		std::thread* m_previewLoader;
		bool m_previewLoaderRunning;
		void m_stopPreviewLoader();
		void m_loadPreview();

		std::vector<FileTreeNode*> m_treeCache;
		void m_clearTree(FileTreeNode* node);
		void m_renderTree(FileTreeNode* node);

		unsigned int m_sortColumn;
		unsigned int m_sortDirection;
		std::vector<FileData> m_content;
		void m_setDirectory(const std::wstring& p, bool addHistory = true);
		void m_sortContent(unsigned int column, unsigned int sortDirection);
		void m_renderContent();

		void m_renderPopups();
		void m_renderFileDialog();
	};

	static const char* GetDefaultFolderIcon();
	static const char* GetDefaultFileIcon();
}