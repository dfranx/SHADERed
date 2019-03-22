#pragma once
#include <string>

namespace ed
{
	class GLSL2HLSL
	{
	public:
		static std::string Transcompile(const std::string& filename);
		static int IsGLSL(const std::string& file);

	private:
		static void m_exec(const std::string& app, const std::string& args = "");
		static void m_clear();
	};
}