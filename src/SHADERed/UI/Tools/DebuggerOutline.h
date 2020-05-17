#pragma once
#include <SHADERed/Objects/Debug/PixelInformation.h>
#include <glm/glm.hpp>

namespace ed {
	class DebuggerOutline {
	public:
		static void RenderPrimitiveOutline(const PixelInformation& pixel, glm::vec2 uiPos, glm::vec2 itemSize, glm::vec2 zoomPos, glm::vec2 zoomSize);
		static void RenderPixelOutline(const PixelInformation& pixel, glm::vec2 uiPos, glm::vec2 itemSize, glm::vec2 zoomPos, glm::vec2 zoomSize);
	};
}