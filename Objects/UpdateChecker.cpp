#include "UpdateChecker.h"
#include <SFML/Network.hpp>
#include <thread>

namespace ed
{
	void checkUpdates(std::function<void()> onUpdate)
	{
		sf::Http http("http://shadered.000webhostapp.com/"); // ye, ik.....
		sf::Http::Request request;
		sf::Http::Response response = http.sendRequest(request, sf::seconds(2.0f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();

			// extra cautious
			bool isAllDigits = true;
			for (int i = 0; i < src.size(); i++)
				if (!isdigit(src[i])) {
					isAllDigits = false;
					break;
				}

			if (isAllDigits && src.size() > 0 && src.size() < 6) {
				int ver = std::stoi(src);
				if (ver > UpdateChecker::MyVersion && onUpdate != nullptr) 
					onUpdate();
			}
		}
	}
	UpdateChecker::UpdateChecker()
	{
		m_thread = nullptr;
	}
	UpdateChecker::~UpdateChecker()
	{
		if (m_thread != nullptr) {
			if (m_thread->joinable())
				m_thread->join();
			delete m_thread;
		}
	}
	void UpdateChecker::CheckForUpdates(std::function<void()> onUpdate)
	{
		if (m_thread != nullptr) {
			if (m_thread->joinable())
				m_thread->join();
			delete m_thread;
		}
		m_thread = new std::thread(checkUpdates, onUpdate);
	}
}