#pragma once
#include <string>
#include "MessageStack.h"
#include "ShaderMacro.h"
#include "ShaderLanguage.h"
#include "ProjectParser.h"

namespace ed
{
	class ShaderTranscompiler
	{
	public:
		/* TODO: enum for shaderType = { 0 -> vertex, 1 -> pixel, 2 -> geometry } */
		static std::string Transcompile(ShaderLanguage inLang, const std::string &filename, int shaderType, const std::string &entry, std::vector<ShaderMacro> &macros, bool gsUsed, MessageStack *msgs, ProjectParser* project);
		static std::string TranscompileSource(ShaderLanguage inLang, const std::string &filename, const std::string &source, int shaderType, const std::string &entry, std::vector<ShaderMacro> &macros, bool gsUsed, MessageStack *msgs, ProjectParser* project);
		static ShaderLanguage GetShaderTypeFromExtension(const std::string& file);
	};
}