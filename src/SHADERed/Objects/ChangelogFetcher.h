#pragma once
#include <functional>
#include <thread>

namespace ed {
	class ChangelogFetcher {
	public:
		ChangelogFetcher();
		~ChangelogFetcher();

		void Fetch(std::function<void(const std::string&)> onFetch);

	private:
		std::thread* m_thread;
	};
}