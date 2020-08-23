#pragma once
#include <string>
#include <thread>
#include <functional>

namespace ed {
	class WebAPI {
	public:
		static const std::string URL;
		static const int InternalVersion = 15;

		WebAPI();
		~WebAPI();
		
		/* max count, index, title, text*/
		void FetchTips(std::function<void(int, int, const std::string&, const std::string&)> onFetch);

		/* changelog content */
		void FetchChangelog(std::function<void(const std::string&)> onFetch);

		/* handle update required */
		void CheckForApplicationUpdates(std::function<void()> onUpdate);

	private:
		// TODO: maybe have one thread + some kind of job queue system... lazy rn to fix this, just copy pasting old code
		std::thread* m_tipsThread;
		std::thread* m_changelogThread;
		std::thread* m_updateThread;
	};
}