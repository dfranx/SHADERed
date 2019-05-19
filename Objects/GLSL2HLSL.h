#pragma once
#include <string>
#include <MoonLight/Base/Logger.h>

namespace ed
{
	class GLSL2HLSL
	{
	public:
		static std::string Transcompile(const std::string& filename, const std::string& entry, ml::Logger* log);
		static int IsGLSL(const std::string& file);

	private:
		static std::string m_exec(const std::string& cmd);
		static void m_clear();
	};
}