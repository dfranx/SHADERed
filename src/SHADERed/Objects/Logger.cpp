#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ed {
	void Logger::Log(const std::string& msg, bool error, const std::string& file, int line)
	{
		if (!Settings::Instance().General.Log)
			return;

		time_t now = time(0);
		tm* ltm = localtime(&now);

		std::stringstream data;

		// time
		data << "[" << std::setw(2) << std::setfill('0') << ltm->tm_hour << ":" << std::setw(2) << std::setfill('0') << ltm->tm_min << ":" << std::setw(2) << std::setfill('0') << ltm->tm_sec << "] ";

		// file and line
		if (file.size() != 0)
			data << "<" << file;

		if (line != -1) {
			if (file.size() == 0)
				data << "<";
			else
				data << " ";
			data << "at line " << line;
		}

		if (file.size() != 0 || line != -1)
			data << "> ";

		// error
		if (error)
			data << "(ERROR) ";

		// message
		data << msg;

		if (Settings::Instance().General.PipeLogsToTerminal)
			std::cout << data.str() << std::endl;

		if (Settings::Instance().General.StreamLogs) {
			std::ofstream log(ed::Settings::Instance().ConvertPath("log.txt"), std::ios_base::app | std::ios_base::out);
			log << data.str() << std::endl;
			log.close();
		} else
			m_msgs.push_back(data.str());
	}
	void Logger::Save()
	{
		if (!Settings::Instance().General.Log || Settings::Instance().General.StreamLogs)
			return;

		time_t now = time(0);
		tm* ltm = localtime(&now);

		std::ofstream file(ed::Settings::Instance().ConvertPath("log.txt"));
		file << "Log -> " << ltm->tm_mday << "." << ltm->tm_mon + 1 << "." << 1900 + ltm->tm_year << "\n";

		for (auto& line : m_msgs)
			file << line << std::endl;

		file.close();
	}
}