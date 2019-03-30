#include "Logger.h"

namespace ed
{
	void Logger::Log(const std::string& str)
	{
		if (Stack == nullptr)
			return;

		if (str.find("[D3D] Failed to compile") != std::string::npos) {
			std::string type = "vertex";
			if (Stack->CurrentItemType == 1)
				type = "pixel";
			else if (Stack->CurrentItemType == 2)
				type = "geometry";
			Stack->Add(MessageStack::Type::Error, Stack->CurrentItem, "Failed to compile " + type + " shader.", -1, Stack->CurrentItemType);
		}
		else if (str.find(".hlsl") != std::string::npos) {
			size_t firstP = str.find_first_of('(');
			size_t lastD = str.find_last_of(':');

			int line = std::stoi(str.substr(firstP + 1, str.find_first_of(',', firstP) - firstP - 1));
			std::string msg = str.substr(lastD + 2, str.size()- lastD-3);

			Stack->Add(MessageStack::Type::Error, Stack->CurrentItem, msg, line, Stack->CurrentItemType);
		}
	}
}