#pragma once
#include <string>
#include <thread>
#include <functional>

namespace ed {
	class WebAPI {
	public:
		static const std::string URL;
		static const int InternalVersion = 27;
		static const char* Version;

		// info that /api/search will return
		struct ShaderResult 
		{
			std::string ID;
			std::string Title;
			std::string Description;
			std::string Owner;
			int Views;
			std::string Language;
		};
		struct PluginResult
		{
			std::string ID;
			std::string Title;
			std::string Description;
			std::string Owner;
			std::string Thumbnail;
			int Downloads;
		};
		struct ThemeResult {
			std::string ID;
			std::string Title;
			std::string Description;
			std::string Owner;
			std::string Thumbnail;
			int Downloads;
		};

		WebAPI();
		~WebAPI();
		
		/* max count, index, title, text*/
		void FetchTips(std::function<void(int, int, const std::string&, const std::string&)> onFetch);

		/* changelog content */
		void FetchChangelog(std::function<void(const std::string&)> onFetch);

		/* called when update required */
		void CheckForApplicationUpdates(std::function<void()> onUpdate);

		/* list of shaders */
		void SearchShaders(const std::string& query, int page, const std::string& sort, const std::string& language, const std::string& owner, bool excludeGodotShaders, bool includeCPPShaders, bool includeRustShaders, std::function<void(const std::vector<ShaderResult>&)> onFetch);
		
		/* download and parse shader thumbnail in another thread, onFetch(pixelData, width, height) */
		void AllocateShaderThumbnail(const std::string& id, std::function<void(unsigned char*, int, int)> onFetch);

		/* download project in temp directory */
		bool DownloadShaderProject(const std::string& id);

		/* search plugins */
		void SearchPlugins(const std::string& query, int page, const std::string& sort, const std::string& owner, std::function<void(const std::vector<PluginResult>&)> onFetch);

		/* decode thumbnail */
		char* DecodeThumbnail(const std::string& base64, size_t& length);

		/* download plugin */
		void DownloadPlugin(const std::string& id);

		/* search themes */
		void SearchThemes(const std::string& query, int page, const std::string& sort, const std::string& owner, std::function<void(const std::vector<ThemeResult>&)> onFetch);

		/* download theme */
		void DownloadTheme(const std::string& id);

		/* get plugin version */
		int GetPluginVersion(const std::string& id);

	private:
		// TODO: maybe have one thread + some kind of job queue system... lazy rn to fix this, just copy pasting old code
		std::thread* m_tipsThread;
		std::thread* m_changelogThread;
		std::thread* m_updateThread;
		std::thread* m_shadersThread;
		std::thread* m_thumbnailThread;
		std::thread* m_themesThread;
		std::thread* m_pluginsThread;
	};
}