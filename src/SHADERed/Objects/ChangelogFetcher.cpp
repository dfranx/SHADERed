#include <SHADERed/Objects/ChangelogFetcher.h>
#include <SHADERed/Objects/UpdateChecker.h>
#include <SFML/Network.hpp>
#include <thread>

namespace ed {
	void getChangelog(std::function<void(const std::string&)> onFetch)
	{
		sf::Http http("http://api.shadered.org");
		sf::Http::Request request;
		request.setUri("/changelog.php");
		request.setBody("version=" + std::to_string(UpdateChecker::MyVersion));
		request.setMethod(sf::Http::Request::Method::Post);
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.5f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();
			onFetch(src);
		}
	}
	ChangelogFetcher::ChangelogFetcher()
	{
		m_thread = nullptr;
	}
	ChangelogFetcher::~ChangelogFetcher()
	{
		if (m_thread != nullptr && m_thread->joinable())
			m_thread->join();
		delete m_thread;
	}
	void ChangelogFetcher::Fetch(std::function<void(const std::string&)> onFetch)
	{
		if (m_thread != nullptr && m_thread->joinable())
			m_thread->join();
		delete m_thread;
		m_thread = new std::thread(getChangelog, onFetch);
	}
}