#pragma once
#include <functional>
#include <thread>

namespace ed {
	class TipFetcher {
	public:
		TipFetcher();
		~TipFetcher();

		/* max count, index, title, text*/
		void Fetch(std::function<void(int, int, const std::string&, const std::string&)> onFetch);

	private:
		std::thread* m_thread;
	};
}