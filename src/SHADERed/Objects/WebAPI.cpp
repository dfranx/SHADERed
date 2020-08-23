#include <SHADERed/Objects/WebAPI.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>

#include <SFML/Network.hpp>
#include <pugixml/src/pugixml.hpp>
#include <miniz/zip_file.hpp>

#include <filesystem>
#include <fstream>
#include <random>

#if defined(_MSC_VER) // Visual studio
#define thread_local __declspec(thread)
#elif defined(__GCC__) // GCC
#define thread_local __thread
#endif


namespace ed {
	const std::string WebAPI::URL = "localhost"; // TODO: change URL & ports

	int threadSafeRandom(int vmin, int vmax)
	{
		static thread_local std::mt19937* generator = nullptr;
		if (!generator) generator = new std::mt19937(clock() + std::hash<std::thread::id>()(std::this_thread::get_id()));
		std::uniform_int_distribution<int> distribution(vmin, vmax);
		return distribution(*generator);
	}
	
	/* actual functions */
	void getTips(std::function<void(int, int, const std::string&, const std::string&)> onFetch)
	{
		sf::Http http(WebAPI::URL, 16001);
		sf::Http::Request request;
		request.setUri("/api/tips.xml");
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.75f));

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

			int index = threadSafeRandom(0, tipCount - 1);

			pugi::xml_node tip;
			int curInd = 0;

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
	void getChangelog(std::function<void(const std::string&)> onFetch)
	{
		sf::Http http(WebAPI::URL, 16001);
		sf::Http::Request request;
		request.setUri("/api/changelog");
		request.setBody("version=" + std::to_string(WebAPI::InternalVersion));
		request.setMethod(sf::Http::Request::Method::Post);
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.5f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();
			onFetch(src);
		}
	}
	void checkUpdates(std::function<void()> onUpdate)
	{
		sf::Http http(WebAPI::URL, 16001);
		sf::Http::Request request;
		request.setUri("/api/version");
		sf::Http::Response response = http.sendRequest(request, sf::seconds(5.0f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();

			bool isAllDigits = true;
			for (int i = 0; i < src.size(); i++)
				if (!isdigit(src[i])) {
					isAllDigits = false;
					break;
				}

			if (isAllDigits && src.size() > 0 && src.size() < 6) {
				int ver = std::stoi(src);
				if (ver > WebAPI::InternalVersion && onUpdate != nullptr)
					onUpdate();
			}
		}
	}
	void searchShaders(const std::string& query, int page, const std::string& sort, const std::string& language, const std::string& owner, bool excludeGodotShaders, std::function<void(std::vector<WebAPI::ShaderResult>)> onFetch)
	{
		std::string requestBody = "query=" + query + "&page=" + std::to_string(page) + "&sort=" + sort + "&language=" + language + "&exclude_godot=" + std::to_string(excludeGodotShaders); 
		if (!owner.empty())
			requestBody += "&owner=" + owner;

		sf::Http http(WebAPI::URL, 16001);
		sf::Http::Request request;
		request.setUri("/api/search");
		request.setBody(requestBody);
		request.setMethod(sf::Http::Request::Method::Post);
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.5f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();

			std::vector<WebAPI::ShaderResult> results;

			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_string(src.c_str());
			if (!result) {
				Logger::Get().Log("Failed to parse web search results", true);
				return;
			}

			auto resultsNode = doc.child("results");

			for (auto shaderNode : resultsNode.children("shader")) {
				WebAPI::ShaderResult result;

				result.ID = shaderNode.child("id").text().as_string();
				result.Title = shaderNode.child("title").text().as_string();
				result.Description = shaderNode.child("description").text().as_string();
				result.Owner = shaderNode.child("owner").text().as_string();
				result.Views = shaderNode.child("views").text().as_int();
				result.Language = shaderNode.child("language").text().as_string();
				
				results.push_back(result);
			}

			onFetch(results);
		}
	}
	
	WebAPI::WebAPI()
	{
		m_tipsThread = nullptr;
		m_changelogThread = nullptr;
		m_updateThread = nullptr;
		m_searchThread = nullptr;
	}
	WebAPI::~WebAPI()
	{
		if (m_tipsThread != nullptr && m_tipsThread->joinable())
			m_tipsThread->join();
		delete m_tipsThread;

		if (m_changelogThread != nullptr && m_changelogThread->joinable())
			m_changelogThread->join();
		delete m_changelogThread;

		if (m_updateThread != nullptr && m_updateThread->joinable())
			m_updateThread->join();
		delete m_updateThread;

		if (m_searchThread != nullptr && m_searchThread->joinable())
			m_searchThread->join();
		delete m_searchThread;
	}
	void WebAPI::FetchTips(std::function<void(int, int, const std::string&, const std::string&)> onFetch)
	{
		std::string currentVersionPath = "info.dat";
		if (!ed::Settings().Instance().LinuxHomeDirectory.empty())
			currentVersionPath = ed::Settings().Instance().LinuxHomeDirectory + currentVersionPath;

		std::ifstream verReader(currentVersionPath);
		int curVer = 0;
		if (verReader.is_open()) {
			verReader >> curVer;
			verReader.close();
		}

		if (curVer < WebAPI::InternalVersion)
			return;

		if (m_tipsThread != nullptr && m_tipsThread->joinable())
			m_tipsThread->join();
		delete m_tipsThread;
		m_tipsThread = new std::thread(getTips, onFetch);
	}
	void WebAPI::FetchChangelog(std::function<void(const std::string&)> onFetch)
	{
		if (m_changelogThread != nullptr && m_changelogThread->joinable())
			m_changelogThread->join();
		delete m_changelogThread;
		m_changelogThread = new std::thread(getChangelog, onFetch);
	}
	void WebAPI::CheckForApplicationUpdates(std::function<void()> onUpdate)
	{
		if (m_updateThread != nullptr && m_updateThread->joinable())
			m_updateThread->join();
		delete m_updateThread;
		m_updateThread = new std::thread(checkUpdates, onUpdate);
	}
	void WebAPI::SearchShaders(const std::string& query, int page, const std::string& sort, const std::string& language, const std::string& owner, bool excludeGodotShaders, std::function<void(std::vector<ShaderResult>)> onFetch)
	{
		if (m_searchThread != nullptr && m_searchThread->joinable())
			m_searchThread->join();
		delete m_searchThread;
		m_searchThread = new std::thread(searchShaders, query, page, sort, language, owner, excludeGodotShaders, onFetch);
	}
	char* WebAPI::AllocateThumbnail(const std::string& id, size_t& length)
	{
		sf::Http http(WebAPI::URL, 16001);
		sf::Http::Request request;
		request.setUri("/thumbnails/" + id + "/0.png");
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.5f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();

			const char* bytedata = src.c_str();
			length = src.size();

			char* allocated = (char*)malloc(length);
			memcpy(allocated, bytedata, length);

			return allocated;
		}

		return nullptr;
	}
	bool WebAPI::DownloadShaderProject(const std::string& id)
	{
		std::string outputPath = "temp/";
		if (!ed::Settings::Instance().LinuxHomeDirectory.empty())
			outputPath = ed::Settings::Instance().LinuxHomeDirectory + "temp/";

		// first clear the old data and create new directory
		std::error_code ec;
		std::filesystem::remove_all(outputPath, ec);
		std::filesystem::create_directory(outputPath);

		sf::Http http(WebAPI::URL, 16001);
		sf::Http::Request request;
		request.setUri("/download?type=shader&id=" + id);
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.5f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();

			miniz_cpp::zip_file zipFile((unsigned char*)src.c_str(), src.size());
			std::vector<std::string> files = zipFile.namelist();
			for (int i = 0; i < files.size(); i++) {
				miniz_cpp::zip_info fileInfo = zipFile.getinfo(files[i]);

				if (fileInfo.flag_bits == 0 && fileInfo.file_size == 0)
					std::filesystem::create_directory(std::filesystem::path(outputPath) / fileInfo.filename);
			}
			zipFile.extractall(outputPath);

			return true;
		}

		return false;
	}
}