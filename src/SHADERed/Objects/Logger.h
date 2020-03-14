#pragma once
#include <SHADERed/Objects/MessageStack.h>
#include <string>

namespace ed {
	class Logger {
	public:
		MessageStack* Stack;

		Logger()
		{
			Stack = nullptr;
		}

		static Logger& Get()
		{
			static Logger ret;
			return ret;
		}

		void Log(const std::string& msg, bool error = false, const std::string& file = "", int line = -1);
		void Save();

	private:
		std::vector<std::string> m_msgs;
	};
}