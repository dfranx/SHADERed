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
		else if (str.find("ERROR") != std::string::npos) {
			/*
			ERROR: #version: ES shaders for Vulkan SPIR-V require version 310 or higher
			ERROR: vertex.vert:10: '' :  syntax error, unexpected IDENTIFIER
			*/

			size_t errLoc = str.find("ERROR");
			std::string error = str.substr(errLoc, str.find_first_of('\n', errLoc + 1)-errLoc+1);

			size_t colon1 = error.find_first_of(':'); // skip filename&path

			if (error[colon1 + 2] == '#') {
				// ERROR: #version : ES shaders for Vulkan SPIR - V require version 310 or higher
				int line = 1;
				std::string msg = error.substr(colon1 + 2, error.size() - colon1 - 2 - 2);

				Stack->Add(MessageStack::Type::Error, Stack->CurrentItem, msg, line, Stack->CurrentItemType);
			}
			else if (error.find("Linking") != std::string::npos)
				Stack->Add(MessageStack::Type::Error, Stack->CurrentItem, "No shader entry", -1, Stack->CurrentItemType);
			else {
				// ERROR: vertex.vert:10: '' :  syntax error, unexpected IDENTIFIER
				size_t colon2 = error.find_first_of(':', colon1 + 4); // +4 -> skip semicolon in path
				size_t colon3 = error.find_first_of(':', colon2 + 1);
				size_t colon4 = error.find_first_of(':', colon3 + 1);

				int line = std::stoi(error.substr(colon2 + 1, colon3 - colon2 - 1));
				std::string msg = error.substr(colon4 +1, error.size()-colon4-1-2);
				msg = msg.substr(msg.find_first_not_of(' '));
				msg = msg.substr(0,msg.find_last_not_of(' '));
				
				Stack->Add(MessageStack::Type::Error, Stack->CurrentItem, msg, line, Stack->CurrentItemType);
			}
		}

	}
}