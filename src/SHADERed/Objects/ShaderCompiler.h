#pragma once
#include <SHADERed/Objects/MessageStack.h>
#include <SHADERed/Objects/ProjectParser.h>
#include <SHADERed/Objects/ShaderLanguage.h>
#include <SHADERed/Objects/ShaderStage.h>
#include <SHADERed/Objects/ShaderMacro.h>
#include <string>

namespace ed {
	class ShaderCompiler {
	public:
		static bool CompileToSPIRV(std::vector<unsigned int>& spvOut, ShaderLanguage inLang, const std::string& filename, ShaderStage shaderType, const std::string& entry, std::vector<ShaderMacro>& macros, MessageStack* msgs, ProjectParser* project);
		static bool CompileSourceToSPIRV(std::vector<unsigned int>& spvOut, ShaderLanguage inLang, const std::string& filename, const std::string& source, ShaderStage shaderType, const std::string& entry, std::vector<ShaderMacro>& macros, MessageStack* msgs, ProjectParser* project);
		static std::string ConvertToGLSL(const std::vector<unsigned int>& spvIn, ShaderLanguage inLang, ShaderStage sType, bool gsUsed, MessageStack* msgs);
		static IPlugin1* GetPluginLanguageFromExtension(int* lang, const std::string& filename, const std::vector<IPlugin1*>& pls);
		static ShaderLanguage GetShaderLanguageFromExtension(const std::string& file);
	};
}