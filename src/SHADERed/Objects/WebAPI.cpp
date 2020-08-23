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
	
	/* base64 stuff for plugin & theme thumbnails */
	/* 
	   base64.cpp and base64.h

	   Copyright (C) 2004-2008 René Nyffenegger

	   This source code is provided 'as-is', without any express or implied
	   warranty. In no event will the author be held liable for any damages
	   arising from the use of this software.

	   Permission is granted to anyone to use this software for any purpose,
	   including commercial applications, and to alter it and redistribute it
	   freely, subject to the following restrictions:

	   1. The origin of this source code must not be misrepresented; you must not
		  claim that you wrote the original source code. If you use this source code
		  in a product, an acknowledgment in the product documentation would be
		  appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
		  misrepresented as being the original source code.

	   3. This notice may not be removed or altered from any source distribution.

	   René Nyffenegger rene.nyffenegger@adp-gmbh.ch

	*/
	static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
											"abcdefghijklmnopqrstuvwxyz"
											"0123456789+/";
	static inline bool is_base64(unsigned char c)
	{
		return (isalnum(c) || (c == '+') || (c == '/'));
	}
	std::string base64_decode(std::string const& encoded_string)
	{
		int in_len = encoded_string.size();
		int i = 0;
		int j = 0;
		int in_ = 0;
		unsigned char char_array_4[4], char_array_3[3];
		std::string ret;

		while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
			char_array_4[i++] = encoded_string[in_];
			in_++;
			if (i == 4) {
				for (i = 0; i < 4; i++)
					char_array_4[i] = base64_chars.find(char_array_4[i]);

				char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
				char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
				char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

				for (i = 0; (i < 3); i++)
					ret += char_array_3[i];
				i = 0;
			}
		}

		if (i) {
			for (j = i; j < 4; j++)
				char_array_4[j] = 0;

			for (j = 0; j < 4; j++)
				char_array_4[j] = base64_chars.find(char_array_4[j]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (j = 0; (j < i - 1); j++)
				ret += char_array_3[j];
		}

		return ret;
	}


	WebAPI::WebAPI()
	{
		m_tipsThread = nullptr;
		m_changelogThread = nullptr;
		m_updateThread = nullptr;
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
	std::vector<WebAPI::ShaderResult> WebAPI::SearchShaders(const std::string& query, int page, const std::string& sort, const std::string& language, const std::string& owner, bool excludeGodotShaders)
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
				return std::vector<WebAPI::ShaderResult>();
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

			return results;
		}

		return std::vector<WebAPI::ShaderResult>();
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
					std::filesystem::create_directories(std::filesystem::path(outputPath) / fileInfo.filename);
			}
			zipFile.extractall(outputPath);

			return true;
		}

		return false;
	}
	std::vector<WebAPI::PluginResult> WebAPI::SearchPlugins(const std::string& query, int page, const std::string& sort, const std::string& owner)
	{
		std::string requestBody = "query=" + query + "&page=" + std::to_string(page) + "&sort=" + sort;
		if (!owner.empty())
			requestBody += "&owner=" + owner;

		sf::Http http(WebAPI::URL, 16001);
		sf::Http::Request request;
		request.setUri("/api/search_plugin");
		request.setBody(requestBody);
		request.setMethod(sf::Http::Request::Method::Post);
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.5f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();

			std::vector<WebAPI::PluginResult> results;

			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_string(src.c_str());
			if (!result) {
				Logger::Get().Log("Failed to parse web search results", true);
				return std::vector<WebAPI::PluginResult>();
			}

			auto resultsNode = doc.child("results");

			for (auto pluginNode : resultsNode.children("plugin")) {
				WebAPI::PluginResult result;

				result.ID = pluginNode.child("id").text().as_string();
				result.Title = pluginNode.child("title").text().as_string();
				result.Description = pluginNode.child("description").text().as_string();
				result.Owner = pluginNode.child("owner").text().as_string();
				result.Thumbnail = pluginNode.child("thumbnail").text().as_string();
				result.Downloads = pluginNode.child("downloads").text().as_int();

				result.Thumbnail = result.Thumbnail.substr(result.Thumbnail.find_first_of(',') + 1);
				
				results.push_back(result);
			}

			return results;
		}

		return std::vector<WebAPI::PluginResult>();
	}
	char* WebAPI::DecodeThumbnail(const std::string& base64, size_t& length)
	{
		std::string decoded = base64_decode(base64);

		const char* bytedata = decoded.c_str();
		length = decoded.size();

		char* allocated = (char*)malloc(length);
		memcpy(allocated, bytedata, length);

		return allocated;
	}
	void WebAPI::DownloadPlugin(const std::string& id)
	{
		std::string body = "/download?type=plugin&id=" + id + "&os=";
#if defined(_WIN64)
		body += "w64";
#elif defined(_WIN32)
		body += "w32";
#elif defined(__linux__) || defined(__unix__)
		body += "lin";
#endif
		
		
		std::string outputDir = ".";
		if (!ed::Settings::Instance().LinuxHomeDirectory.empty())
			outputDir = ed::Settings::Instance().LinuxHomeDirectory;

		sf::Http http(WebAPI::URL, 16001);
		sf::Http::Request request;
		request.setUri(body);
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.5f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();

			miniz_cpp::zip_file zipFile((unsigned char*)src.c_str(), src.size());
			std::vector<std::string> files = zipFile.namelist();
			for (int i = 0; i < files.size(); i++) {
				miniz_cpp::zip_info fileInfo = zipFile.getinfo(files[i]);

				if (fileInfo.flag_bits == 0 && fileInfo.file_size == 0)
					std::filesystem::create_directories(std::filesystem::path(outputDir) / fileInfo.filename);
			}
			zipFile.extractall(outputDir);
		}
	}
	std::vector<WebAPI::ThemeResult> WebAPI::SearchThemes(const std::string& query, int page, const std::string& sort, const std::string& owner)
	{
		std::string requestBody = "query=" + query + "&page=" + std::to_string(page) + "&sort=" + sort;
		if (!owner.empty())
			requestBody += "&owner=" + owner;

		sf::Http http(WebAPI::URL, 16001);
		sf::Http::Request request;
		request.setUri("/api/search_theme");
		request.setBody(requestBody);
		request.setMethod(sf::Http::Request::Method::Post);
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.5f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();

			std::vector<WebAPI::ThemeResult> results;

			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_string(src.c_str());
			if (!result) {
				Logger::Get().Log("Failed to parse web search results", true);
				return std::vector<WebAPI::ThemeResult>();
			}

			auto resultsNode = doc.child("results");

			for (auto pluginNode : resultsNode.children("theme")) {
				WebAPI::ThemeResult result;

				result.ID = pluginNode.child("id").text().as_string();
				result.Title = pluginNode.child("title").text().as_string();
				result.Description = pluginNode.child("description").text().as_string();
				result.Owner = pluginNode.child("owner").text().as_string();
				result.Thumbnail = pluginNode.child("thumbnail").text().as_string();
				result.Downloads = pluginNode.child("downloads").text().as_int();

				result.Thumbnail = result.Thumbnail.substr(result.Thumbnail.find_first_of(',') + 1);

				results.push_back(result);
			}

			return results;
		}

		return std::vector<WebAPI::ThemeResult>();
	}
	void WebAPI::DownloadTheme(const std::string& id)
	{
		std::string outputPath = "./themes/";
		if (!Settings().Instance().LinuxHomeDirectory.empty())
			outputPath = Settings().Instance().LinuxHomeDirectory + "themes/";

		sf::Http http(WebAPI::URL, 16001);
		sf::Http::Request request;
		request.setUri("/download?type=theme&id=" + id);
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.5f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();

			std::ofstream outFile(outputPath + id + ".ini");
			outFile << src;
			outFile.close();
		}
	}
	int WebAPI::GetPluginVersion(const std::string& id)
	{
		sf::Http http(WebAPI::URL, 16001);
		sf::Http::Request request;
		request.setUri("/api/get_plugin_version?id=" + id);
		sf::Http::Response response = http.sendRequest(request, sf::seconds(0.5f));

		if (response.getStatus() == sf::Http::Response::Ok) {
			std::string src = response.getBody();
			return std::stoi(src);
		}

		return 0;
	}
}