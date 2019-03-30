#pragma once
#include "MessageStack.h"
#include <MoonLight/Base/Logger.h>
#include <string>

namespace ed
{
	class Logger : public ml::Logger
	{
	public:
		MessageStack* Stack;

		Logger() {
			Stack = nullptr;
		}

		virtual void Log(const std::string& str);
	};
}