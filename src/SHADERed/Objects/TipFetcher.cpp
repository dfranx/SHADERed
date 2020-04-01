#include <SHADERed/Objects/TipFetcher.h>
#include <SHADERed/Objects/UpdateChecker.h>
#include <SHADERed/Objects/Logger.h>
#include <SFML/Network.hpp>

#include <pugixml/src/pugixml.hpp>
#include <fstream>
#include <thread>
#include <random>

#if defined(_MSC_VER) // Visual studio
#define thread_local __declspec(thread)
#elif defined(__GCC__) // GCC
#define thread_local __thread
#endif

namespace ed {
	/* https://stackoverflow.com/questions/21237905/how-do-i-generate-thread-safe-uniform-random-numbers */
	int intRand(const int& min, const int& max)
	{
		static thread_local std::mt19937* generator = nullptr;
		if (!generator) generator = new std::mt19937(clock() + std::hash<std::thread::id>()(std::this_thread::get_id()));
		std::uniform_int_distribution<int> distribution(min, max);
		return distribution(*generator);
	}

	void getTips(std::function<void(int, int, const std::string&, const std::string&)> onFetch)
	{
		sf::Http http("http://api.shadered.org");
		sf::Http::Request request;
		request.setUri("/tips.xml");
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.5f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();
			
			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_string(src.c_str());
			if (!result) {
				Logger::Get().Log("Failed to parse tips.xml", true);
				return;
			}
			
			auto tips = doc.child("tips");
			int tipCount = std::distance(tips.children("tip").begin(), tips.children("tip").end());

			int index = intRand(0, tipCount-1);

			pugi::xml_node tip;
			int curInd = 0;

			// TODO: why can't i access elements using the index??
			for (auto cTip : tips.children("tip")) {
				if (curInd == index) {
					tip = cTip;
					break;
				}

				curInd++;
			}

			std::string tipTitle(tip.child("title").text().as_string());
			std::string tipText(tip.child("text").text().as_string());
			onFetch(tipCount, index, tipTitle, tipText);
		}
	}
	TipFetcher::TipFetcher()
	{
		m_thread = nullptr;
	}
	TipFetcher::~TipFetcher()
	{
		if (m_thread != nullptr && m_thread->joinable())
			m_thread->join();
		delete m_thread;
	}
	void TipFetcher::Fetch(std::function<void(int, int, const std::string&, const std::string&)> onFetch)
	{
		std::ifstream verReader("current_version.txt");
		int curVer = 0;
		if (verReader.is_open()) {
			verReader >> curVer;
			verReader.close();
		}

		if (curVer < UpdateChecker::MyVersion)
			return;

		if (m_thread != nullptr && m_thread->joinable())
			m_thread->join();
		delete m_thread;
		m_thread = new std::thread(getTips, onFetch);
	}
}