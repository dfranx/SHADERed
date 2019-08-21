#pragma once
#include <string>
#include "MessageStack.h"

namespace ed
{
	class HLSL2GLSL
	{
	public:
		/* shaderType = { 0 -> vertex, 1 -> pixel, 2 -> geometry } */
		static std::string Transcompile(const std::string& filename, int shaderType, const std::string& entry, bool gsUsed, MessageStack* msgs);
		static bool IsHLSL(const std::string& file);
	};
}