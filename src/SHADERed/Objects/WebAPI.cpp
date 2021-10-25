#include <SHADERed/Objects/WebAPI.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>

#include <pugixml/src/pugixml.hpp>
#include <misc/zip_file.hpp>
#include <misc/stb_image.h>
#define CPPHTTPLIB_CONNECTION_TIMEOUT_SECOND 3
#include <misc/httplib.h>

#include <filesystem>
#include <fstream>
#include <random>

#if defined(_MSC_VER) // Visual studio
#define thread_local __declspec(thread)
#elif defined(__GCC__) // GCC
#define thread_local __thread
#endif


namespace ed {
	const std::string WebAPI::URL = "http://api.shadered.org";
	const char* WebAPI::Version = "1.5.6";

	bool isDigitsOnly(const std::string& str)
	{
		for (int i = 0; i < str.size(); i++)
			if (!isdigit(str[i]))
				return false;
		return true;
	}
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
		httplib::Client cli(WebAPI::URL.c_str());
		auto res = cli.Get("/tips.xml");

		if (res && res->status == 200) {
			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_string(res->body.c_str());
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
		httplib::Client cli(WebAPI::URL.c_str());
		httplib::Params params { { "version", std::to_string(WebAPI::InternalVersion) } };
		auto res = cli.Post("/changelog.php", params);

		if (res && res->status == 200)
			onFetch(res->body);
	}
	void checkUpdates(std::function<void()> onUpdate)
	{
		httplib::Client cli(WebAPI::URL.c_str());
		auto res = cli.Get("/version.php");

		if (res && res->status == 200) {
			const std::string& src = res->body;

			if (src.size() > 0 && src.size() < 6 && isDigitsOnly(src)) {
				int ver = std::stoi(src);
				if (ver > WebAPI::InternalVersion && onUpdate != nullptr)
					onUpdate();
			}
		}
	}
	void searchShaders(const std::string& query, int page, const std::string& sort, const std::string& language, const std::string& owner, bool excludeGodotShaders, bool includeCPPShaders, bool includeRustShaders, std::function<void(const std::vector<WebAPI::ShaderResult>&)> onFetch)
	{
		std::vector<WebAPI::ShaderResult> ret = std::vector<WebAPI::ShaderResult>();

		httplib::Params params {
			{ "query", query },
			{ "page", std::to_string(page) },
			{ "sort", sort },
			{ "language", language },
			{ "exclude_godot", std::to_string(excludeGodotShaders) },
			{ "include_cpp", std::to_string(includeCPPShaders) },
			{ "include_rust", std::to_string(includeRustShaders) }
		};
		if (!owner.empty())
			params.emplace("owner", owner);

		httplib::Client cli(WebAPI::URL.c_str());
		auto res = cli.Post("/search.php", params);

		if (res && res->status == 200) {
			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_string(res->body.c_str());
			if (result) {
				auto resultsNode = doc.child("results");

				for (auto shaderNode : resultsNode.children("shader")) {
					WebAPI::ShaderResult result;

					result.ID = shaderNode.child("id").text().as_string();
					result.Title = shaderNode.child("title").text().as_string();
					result.Description = shaderNode.child("description").text().as_string();
					result.Owner = shaderNode.child("owner").text().as_string();
					result.Views = shaderNode.child("views").text().as_int();
					result.Language = shaderNode.child("language").text().as_string();

					ret.push_back(result);
				}
			}
		}

		onFetch(ret);
	}
	void searchThemes(const std::string& query, int page, const std::string& sort, const std::string& owner, std::function<void(const std::vector<WebAPI::ThemeResult>&)> onFetch)
	{
		std::vector<WebAPI::ThemeResult> ret = std::vector<WebAPI::ThemeResult>();
		
		httplib::Params params {
			{ "query", query },
			{ "page", std::to_string(page) },
			{ "sort", sort }
		};
		if (!owner.empty())
			params.emplace("owner", owner);

		httplib::Client cli(WebAPI::URL.c_str());
		auto res = cli.Post("/search_theme.php", params);

		if (res && res->status == 200) {
			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_string(res->body.c_str());
			if (result) {
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

					ret.push_back(result);
				}
			}
		}

		onFetch(ret);
	}
	void searchPlugins(const std::string& query, int page, const std::string& sort, const std::string& owner, std::function<void(const std::vector<WebAPI::PluginResult>&)> onFetch)
	{
		std::vector<WebAPI::PluginResult> ret = std::vector<WebAPI::PluginResult>();

		httplib::Params params {
			{ "query", query },
			{ "page", std::to_string(page) },
			{ "sort", sort }
		};
		if (!owner.empty())
			params.emplace("owner", owner);

		httplib::Client cli(WebAPI::URL.c_str());

		auto res = cli.Post("/search_plugin.php", params);

		if (res && res->status == 200) {
			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_string(res->body.c_str());
			if (result) {
				auto resultsNode = doc.child("results");

				for (auto pluginNode : resultsNode.children("plugin")) {

					// first check if plugin supports our OS
					int flags = pluginNode.child("flags").text().as_int();
					bool shouldAdd = false;

#if defined(_WIN64)
					if (flags & 0b0001)
						shouldAdd = true; // plugin supports win64
#elif defined(_WIN32)
					if (flags & 0b0010)
						shouldAdd = true; // plugin supports win32
#elif defined(__linux__) || defined(__unix__)
					if (flags & 0b0100)
						shouldAdd = true; // plugin supports linux
#endif
					if (!shouldAdd)
						continue;



					WebAPI::PluginResult result;

					result.ID = pluginNode.child("id").text().as_string();
					result.Title = pluginNode.child("title").text().as_string();
					result.Description = pluginNode.child("description").text().as_string();
					result.Owner = pluginNode.child("owner").text().as_string();
					result.Thumbnail = pluginNode.child("thumbnail").text().as_string();
					result.Downloads = pluginNode.child("downloads").text().as_int();

					result.Thumbnail = result.Thumbnail.substr(result.Thumbnail.find_first_of(',') + 1);

					ret.push_back(result);
				}
			}
		}

		onFetch(ret);
	}
	void allocateThumbnail(const std::string& id, std::function<void(unsigned char*, int, int)> onFetch)
	{
		httplib::Client cli(WebAPI::URL.c_str());

		httplib::Params params { { "shader", id } };
		auto response = cli.Post("/get_shader_thumbnail.php", params);

		unsigned char* imgData = nullptr;
		int imgWidth = 0, imgHeight = 0;

		if (response && response->status == 200) {
			const char* byteData = response->body.c_str();
			size_t byteLength = response->body.size();

			int nrChannels;
			imgData = stbi_load_from_memory((stbi_uc*)byteData, byteLength, &imgWidth, &imgHeight, &nrChannels, STBI_rgb_alpha);

			if (imgData == nullptr) {
				imgWidth = 0;
				imgHeight = 0;
			}
		}

		onFetch(imgData, imgWidth, imgHeight);
	}

	/* base64 stuff for plugin & theme thumbnails */
	/* 
	   base64.cpp and base64.h

	   Copyright (C) 2004-2008 Ren� Nyffenegger

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

	   Ren� Nyffenegger rene.nyffenegger@adp-gmbh.ch

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
		m_shadersThread = nullptr;
		m_thumbnailThread = nullptr;
		m_themesThread = nullptr;
		m_pluginsThread = nullptr;
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

		if (m_shadersThread != nullptr && m_shadersThread->joinable())
			m_shadersThread->join();
		delete m_shadersThread;

		if (m_thumbnailThread != nullptr && m_thumbnailThread->joinable())
			m_thumbnailThread->join();
		delete m_thumbnailThread;

		if (m_themesThread != nullptr && m_themesThread->joinable())
			m_themesThread->join();
		delete m_themesThread;

		if (m_pluginsThread != nullptr && m_pluginsThread->joinable())
			m_pluginsThread->join();
		delete m_pluginsThread;
	}
	void WebAPI::FetchTips(std::function<void(int, int, const std::string&, const std::string&)> onFetch)
	{
		Logger::Get().Log("Fetching tips with WebAPI");

		std::string currentVersionPath = Settings::Instance().ConvertPath("info.dat");

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
		Logger::Get().Log("Fetching changelog with WebAPI");

		if (m_changelogThread != nullptr && m_changelogThread->joinable())
			m_changelogThread->join();
		delete m_changelogThread;
		m_changelogThread = new std::thread(getChangelog, onFetch);
	}
	void WebAPI::CheckForApplicationUpdates(std::function<void()> onUpdate)
	{
		Logger::Get().Log("Checking for updates with WebAPI");

		if (m_updateThread != nullptr && m_updateThread->joinable())
			m_updateThread->join();
		delete m_updateThread;
		m_updateThread = new std::thread(checkUpdates, onUpdate);
	}
	void WebAPI::SearchShaders(const std::string& query, int page, const std::string& sort, const std::string& language, const std::string& owner, bool excludeGodotShaders, bool includeCPPShaders, bool includeRustShaders, std::function<void(const std::vector<ShaderResult>&)> onFetch)
	{
		Logger::Get().Log("Searching shaders with WebAPI");

		if (m_shadersThread != nullptr && m_shadersThread->joinable())
			m_shadersThread->join();
		delete m_shadersThread;
		m_shadersThread = new std::thread(searchShaders, query, page, sort, language, owner, excludeGodotShaders, includeCPPShaders, includeRustShaders, onFetch);
	}
	void WebAPI::AllocateShaderThumbnail(const std::string& id, std::function<void(unsigned char*, int, int)> onFetch)
	{
		if (m_thumbnailThread != nullptr && m_thumbnailThread->joinable())
			m_thumbnailThread->join();
		delete m_thumbnailThread;
		m_thumbnailThread = new std::thread(allocateThumbnail, id, onFetch);
	}
	bool WebAPI::DownloadShaderProject(const std::string& id)
	{
		Logger::Get().Log("Downloading shader project with WebAPI");

		std::string outputPath = Settings::Instance().ConvertPath("temp/");

		// first clear the old data and create new directory
		std::error_code ec;
		std::filesystem::remove_all(outputPath, ec);
		std::filesystem::create_directory(outputPath);

		httplib::Client cli(WebAPI::URL.c_str());
		auto response = cli.Get(("/download.php?type=shader&id=" + id).c_str());

		if (response && response->status == 200) {
			const std::string& src = response->body;

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
	void WebAPI::SearchPlugins(const std::string& query, int page, const std::string& sort, const std::string& owner, std::function<void(const std::vector<WebAPI::PluginResult>&)> onFetch)
	{
		Logger::Get().Log("Searching SHADERed plugins with WebAPI");

		if (m_pluginsThread != nullptr && m_pluginsThread->joinable())
			m_pluginsThread->join();
		delete m_pluginsThread;
		m_pluginsThread = new std::thread(searchPlugins, query, page, sort, owner, onFetch);
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
		Logger::Get().Log("Downloading a plugin with WebAPI");

		std::string body = "/download?type=plugin&id=" + id + "&os=";
#if defined(_WIN64)
		body += "w64";
#elif defined(_WIN32)
		body += "w32";
#elif defined(__linux__) || defined(__unix__)
		body += "lin";
#endif
		
		std::string outputDir = Settings::Instance().ConvertPath(".");

		httplib::Client cli(WebAPI::URL.c_str());
		auto response = cli.Get(body.c_str());

		if (response && response->status == 200) {
			const std::string& src = response->body;

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
	void WebAPI::SearchThemes(const std::string& query, int page, const std::string& sort, const std::string& owner, std::function<void(const std::vector<ThemeResult>&)> onFetch)
	{
		Logger::Get().Log("Searching SHADERed themes with WebAPI");

		if (m_themesThread != nullptr && m_themesThread->joinable())
			m_themesThread->join();
		delete m_themesThread;
		
		m_themesThread = new std::thread(searchThemes, query, page, sort, owner, onFetch);
	}
	void WebAPI::DownloadTheme(const std::string& id)
	{
		Logger::Get().Log("Downloading a theme with WebAPI");

		std::string outputPath = Settings::Instance().ConvertPath("themes/");

		httplib::Client cli(WebAPI::URL.c_str());
		auto response = cli.Get(("/download?type=theme&id=" + id).c_str());

		if (response && response->status == 200) {
			std::ofstream outFile(outputPath + id + ".ini");
			outFile << response->body;
			outFile.close();
		}
	}
	int WebAPI::GetPluginVersion(const std::string& id)
	{
		httplib::Client cli(WebAPI::URL.c_str());
		auto response = cli.Get(("/get_plugin_version.php?id=" + id).c_str());

		if (response && response->status == 200) {
			const std::string& src = response->body;

			if (src.size() > 0 && src.size() < 6 && isDigitsOnly(src))
				return std::stoi(src);
		}

		return 0;
	}
}