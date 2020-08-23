#pragma once
#include <string>
#include <thread>
#include <functional>

namespace ed {
	class WebAPI {
	public:
		static const std::string URL;
		static const int InternalVersion = 15;

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

		WebAPI();
		~WebAPI();
		
		/* max count, index, title, text*/
		void FetchTips(std::function<void(int, int, const std::string&, const std::string&)> onFetch);

		/* changelog content */
		void FetchChangelog(std::function<void(const std::string&)> onFetch);

		/* called when update required */
		void CheckForApplicationUpdates(std::function<void()> onUpdate);

		/* list of shaders */
		void SearchShaders(const std::string& query, int page, const std::string& sort, const std::string& language, const std::string& owner, std::function<void(std::vector<ShaderResult>)> onFetch);

		/* bytes, bytecount <- download thumbnail */
		char* AllocateThumbnail(const std::string& id, size_t& length);

	private:
		// TODO: maybe have one thread + some kind of job queue system... lazy rn to fix this, just copy pasting old code
		std::thread* m_tipsThread;
		std::thread* m_changelogThread;
		std::thread* m_updateThread;
		std::thread* m_searchThread;
	};
}