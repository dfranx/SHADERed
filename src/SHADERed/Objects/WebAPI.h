#pragma once
#include <string>
#include <thread>
#include <functional>

namespace ed {
	class WebAPI {
	public:
		static const std::string URL;
		static const int InternalVersion = 16;

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
		std::vector<ShaderResult> SearchShaders(const std::string& query, int page, const std::string& sort, const std::string& language, const std::string& owner, bool excludeGodotShaders);

		/* bytes, bytecount <- download thumbnail */
		char* AllocateThumbnail(const std::string& id, size_t& length);

		/* download project in temp directory */
		bool DownloadShaderProject(const std::string& id);

		/* search plugins */
		std::vector<PluginResult> SearchPlugins(const std::string& query, int page, const std::string& sort, const std::string& owner);

		/* decode thumbnail */
		char* DecodeThumbnail(const std::string& base64, size_t& length);

		/* download plugin */
		void DownloadPlugin(const std::string& id);

		/* search themes */
		std::vector<ThemeResult> SearchThemes(const std::string& query, int page, const std::string& sort, const std::string& owner);

		/* download theme */
		void DownloadTheme(const std::string& id);

		/* get plugin version */
		int GetPluginVersion(const std::string& id);

	private:
		// TODO: maybe have one thread + some kind of job queue system... lazy rn to fix this, just copy pasting old code
		std::thread* m_tipsThread;
		std::thread* m_changelogThread;
		std::thread* m_updateThread;
	};
}