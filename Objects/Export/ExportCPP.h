#pragma once
#include "../../InterfaceManager.h"

namespace ed
{
	class ExportCPP
	{
	public:
		static bool Export(InterfaceManager* data, const std::string& outPath, bool externalShaders = true);

	};
}