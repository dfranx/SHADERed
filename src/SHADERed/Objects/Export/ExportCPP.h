#pragma once
#include <SHADERed/InterfaceManager.h>

namespace ed {
	class ExportCPP {
	public:
		static bool Export(InterfaceManager* data, const std::string& outPath, bool externalShaders, bool exportCmakeFiles, const std::string& cmakeProject, bool copyCMakeModules, bool copySTBImage, bool copyImages);
	};
}