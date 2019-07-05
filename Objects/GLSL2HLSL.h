#pragma once
#include <string>
#include <MoonLight/Base/Logger.h>

namespace ed
{
	class GLSL2HLSL
	{
	public:
		/* shaderType = { 0 -> vertex, 1 -> pixel, 2 -> geometry } */
		static std::string Transcompile(const std::string& filename, int shaderType, const std::string& entry, ml::Logger* log);
		static bool IsGLSL(const std::string& file);

	private:
		static std::string m_exec(const std::string& cmd);
		static void m_clear();
	};
}