#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <algorithm>

#include "Logger.h"
#include "Settings.h"
#include "HLSL2GLSL.h"
#include <glslang/glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/StandAlone/DirStackFileIncluder.h>
#include <SPIRVCross/spirv_glsl.hpp>
#include <SPIRVCross/spirv_cross_util.hpp>
#include "../Engine/GLUtils.h"


const TBuiltInResource DefaultTBuiltInResource = {
	/* .MaxLights = */ 32,
	/* .MaxClipPlanes = */ 6,
	/* .MaxTextureUnits = */ 32,
	/* .MaxTextureCoords = */ 32,
	/* .MaxVertexAttribs = */ 64,
	/* .MaxVertexUniformComponents = */ 4096,
	/* .MaxVaryingFloats = */ 64,
	/* .MaxVertexTextureImageUnits = */ 32,
	/* .MaxCombinedTextureImageUnits = */ 80,
	/* .MaxTextureImageUnits = */ 32,
	/* .MaxFragmentUniformComponents = */ 4096,
	/* .MaxDrawBuffers = */ 32,
	/* .MaxVertexUniformVectors = */ 128,
	/* .MaxVaryingVectors = */ 8,
	/* .MaxFragmentUniformVectors = */ 16,
	/* .MaxVertexOutputVectors = */ 16,
	/* .MaxFragmentInputVectors = */ 15,
	/* .MinProgramTexelOffset = */ -8,
	/* .MaxProgramTexelOffset = */ 7,
	/* .MaxClipDistances = */ 8,
	/* .MaxComputeWorkGroupCountX = */ 65535,
	/* .MaxComputeWorkGroupCountY = */ 65535,
	/* .MaxComputeWorkGroupCountZ = */ 65535,
	/* .MaxComputeWorkGroupSizeX = */ 1024,
	/* .MaxComputeWorkGroupSizeY = */ 1024,
	/* .MaxComputeWorkGroupSizeZ = */ 64,
	/* .MaxComputeUniformComponents = */ 1024,
	/* .MaxComputeTextureImageUnits = */ 16,
	/* .MaxComputeImageUniforms = */ 8,
	/* .MaxComputeAtomicCounters = */ 8,
	/* .MaxComputeAtomicCounterBuffers = */ 1,
	/* .MaxVaryingComponents = */ 60,
	/* .MaxVertexOutputComponents = */ 64,
	/* .MaxGeometryInputComponents = */ 64,
	/* .MaxGeometryOutputComponents = */ 128,
	/* .MaxFragmentInputComponents = */ 128,
	/* .MaxImageUnits = */ 8,
	/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/* .MaxCombinedShaderOutputResources = */ 8,
	/* .MaxImageSamples = */ 0,
	/* .MaxVertexImageUniforms = */ 0,
	/* .MaxTessControlImageUniforms = */ 0,
	/* .MaxTessEvaluationImageUniforms = */ 0,
	/* .MaxGeometryImageUniforms = */ 0,
	/* .MaxFragmentImageUniforms = */ 8,
	/* .MaxCombinedImageUniforms = */ 8,
	/* .MaxGeometryTextureImageUnits = */ 16,
	/* .MaxGeometryOutputVertices = */ 256,
	/* .MaxGeometryTotalOutputComponents = */ 1024,
	/* .MaxGeometryUniformComponents = */ 1024,
	/* .MaxGeometryVaryingComponents = */ 64,
	/* .MaxTessControlInputComponents = */ 128,
	/* .MaxTessControlOutputComponents = */ 128,
	/* .MaxTessControlTextureImageUnits = */ 16,
	/* .MaxTessControlUniformComponents = */ 1024,
	/* .MaxTessControlTotalOutputComponents = */ 4096,
	/* .MaxTessEvaluationInputComponents = */ 128,
	/* .MaxTessEvaluationOutputComponents = */ 128,
	/* .MaxTessEvaluationTextureImageUnits = */ 16,
	/* .MaxTessEvaluationUniformComponents = */ 1024,
	/* .MaxTessPatchComponents = */ 120,
	/* .MaxPatchVertices = */ 32,
	/* .MaxTessGenLevel = */ 64,
	/* .MaxViewports = */ 16,
	/* .MaxVertexAtomicCounters = */ 0,
	/* .MaxTessControlAtomicCounters = */ 0,
	/* .MaxTessEvaluationAtomicCounters = */ 0,
	/* .MaxGeometryAtomicCounters = */ 0,
	/* .MaxFragmentAtomicCounters = */ 8,
	/* .MaxCombinedAtomicCounters = */ 8,
	/* .MaxAtomicCounterBindings = */ 1,
	/* .MaxVertexAtomicCounterBuffers = */ 0,
	/* .MaxTessControlAtomicCounterBuffers = */ 0,
	/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
	/* .MaxGeometryAtomicCounterBuffers = */ 0,
	/* .MaxFragmentAtomicCounterBuffers = */ 1,
	/* .MaxCombinedAtomicCounterBuffers = */ 1,
	/* .MaxAtomicCounterBufferSize = */ 16384,
	/* .MaxTransformFeedbackBuffers = */ 4,
	/* .MaxTransformFeedbackInterleavedComponents = */ 64,
	/* .MaxCullDistances = */ 8,
	/* .MaxCombinedClipAndCullDistances = */ 8,
	/* .MaxSamples = */ 4,

	/* .maxMeshOutputVerticesNV = */ 256,
	/* .maxMeshOutputPrimitivesNV = */ 512,
	/* .maxMeshWorkGroupSizeX_NV = */ 32,
	/* .maxMeshWorkGroupSizeY_NV = */ 1,
	/* .maxMeshWorkGroupSizeZ_NV = */ 1,
	/* .maxTaskWorkGroupSizeX_NV = */ 32,
	/* .maxTaskWorkGroupSizeY_NV = */ 1,
	/* .maxTaskWorkGroupSizeZ_NV = */ 1,
	/* .maxMeshViewCountNV = */ 4,

	/* .limits = */ {
		/* .nonInductiveForLoops = */ 1,
		/* .whileLoops = */ 1,
		/* .doWhileLoops = */ 1,
		/* .generalUniformIndexing = */ 1,
		/* .generalAttributeMatrixVectorIndexing = */ 1,
		/* .generalVaryingIndexing = */ 1,
		/* .generalSamplerIndexing = */ 1,
		/* .generalVariableIndexing = */ 1,
		/* .generalConstantMatrixVectorIndexing = */ 1,
	}
};

namespace ed
{
	std::string HLSL2GLSL::Transcompile(const std::string& filename, int sType, const std::string& entry, std::vector<ShaderMacro>& macros, bool gsUsed, MessageStack* msgs)
	{
		ed::Logger::Get().Log("Starting to transcompile a HLSL shader " + filename);

		//Load HLSL into a string
		std::ifstream file(filename);

		if (!file.is_open())
		{
			msgs->Add(MessageStack::Type::Error, msgs->CurrentItem, "Failed to open file " + filename, -1, sType);
			file.close();
			return "errorFile";
		}

		std::string inputHLSL((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());

		file.close();

		return HLSL2GLSL::TranscompileSource(filename, inputHLSL, sType, entry, macros, gsUsed, msgs);
	}
	std::string HLSL2GLSL::TranscompileSource(const std::string& filename, const std::string& inputHLSL, int sType, const std::string& entry, std::vector<ShaderMacro>& macros, bool gsUsed, MessageStack* msgs)
	{
		// i know, i know... this fix only covers these hardcoded situations
		// but i am waiting for a fix for this issue 
		// https://github.com/KhronosGroup/glslang/issues/1903
		if (inputHLSL.find("float2()") != std::string::npos || inputHLSL.find("float2( )") != std::string::npos ||
			inputHLSL.find("float3()") != std::string::npos || inputHLSL.find("float3( )") != std::string::npos ||
			inputHLSL.find("float4()") != std::string::npos || inputHLSL.find("float4( )") != std::string::npos)
		{
			msgs->Add(MessageStack::Type::Error, msgs->CurrentItem, "Empty constructors are not supported by glslang for now...", -1, sType);
			return "errorGlslang";
		}

		const char* inputStr = inputHLSL.c_str();

		// create shader
		EShLanguage shaderType = EShLangVertex;
		if (sType == 1)
			shaderType = EShLangFragment;
		else if (sType == 2)
			shaderType = EShLangGeometry;
		else if (sType == 3)
			shaderType = EShLangCompute;

		glslang::TShader shader(shaderType);
		if (entry.size() > 0 && entry != "main") {
			shader.setEntryPoint(entry.c_str());
			shader.setSourceEntryPoint(entry.c_str());
		}
		shader.setStrings(&inputStr, 1);

		// set macros
		std::string macroStr = "";
		for (auto& macro : macros) {
			if (!macro.Active)
				continue;
			macroStr += "#define " + std::string(macro.Name) + " " + std::string(macro.Value) + "\n";
		}
		if (macroStr.size() > 0)
			shader.setPreamble(macroStr.c_str());

		// set up
		int sVersion = (sType == 3) ? 430 : 330;
		glslang::EShTargetClientVersion vulkanVersion = glslang::EShTargetVulkan_1_0;
		glslang::EShTargetLanguageVersion targetVersion = glslang::EShTargetSpv_1_0;

		shader.setEnvInput(glslang::EShSourceHlsl, shaderType, glslang::EShClientVulkan, sVersion);
		shader.setEnvClient(glslang::EShClientVulkan, vulkanVersion);
		shader.setEnvTarget(glslang::EShTargetSpv, targetVersion);
		
		TBuiltInResource res = DefaultTBuiltInResource;
		EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

		const int defVersion = sVersion;

		// includer
		DirStackFileIncluder includer;
		includer.pushExternalLocalDirectory(filename.substr(0, filename.find_last_of("/\\")));

		std::string processedHLSL;

		if (!shader.preprocess(&res, defVersion, ENoProfile, false, false, messages, &processedHLSL, includer))
		{
			msgs->Add(MessageStack::Type::Error, msgs->CurrentItem, "HLSL preprocessing failed", -1, sType);
			return "errorPreprocess";
		}

		// update strings
		const char* processedStr = processedHLSL.c_str();
		shader.setStrings(&processedStr, 1);

		// parse
		if (!shader.parse(&res, 100, false, messages))
		{
			msgs->Add(gl::ParseHLSLMessages(msgs->CurrentItem, sType, shader.getInfoLog()));
			return "errorParse";
		}

		// link
		glslang::TProgram prog;
		prog.addShader(&shader);

		if (!prog.link(messages))
		{
			msgs->Add(MessageStack::Type::Error, msgs->CurrentItem, "HLSL linking failed", -1, sType);
			return "errorLink";
		}

		// convert to spirv
		std::vector<unsigned int> spv;
		spv::SpvBuildLogger logger;
		glslang::SpvOptions spvOptions;

		spvOptions.optimizeSize = false;
		spvOptions.disableOptimizer = true;

		glslang::GlslangToSpv(*prog.getIntermediate(shaderType), spv, &logger, &spvOptions);




		// Read SPIR-V from disk or similar.
		spirv_cross::CompilerGLSL glsl(std::move(spv));

		// Set some options.
		spirv_cross::CompilerGLSL::Options options;
		options.version = sVersion;

		glsl.set_common_options(options);

		auto active = glsl.get_active_interface_variables();

		// rename outputs
		spirv_cross::ShaderResources resources = glsl.get_shader_resources();
		std::string outputName = "outputVS";
		if (shaderType == EShLangFragment)
			outputName = "outputPS";
		else if (shaderType == EShLangGeometry)
			outputName = "outputGS";
		for (auto& resource : resources.stage_outputs)
		{
			uint32_t resID = glsl.get_decoration(resource.id, spv::DecorationLocation);
			glsl.set_name(resource.id, outputName + std::to_string(resID));
		}

		// rename inputs
		std::string inputName = "inputVS";
		if (shaderType == EShLangFragment)
			inputName = gsUsed ? "outputGS" : "outputVS";
		else if (shaderType == EShLangGeometry)
			inputName = "outputVS";
		for (auto& resource : resources.stage_inputs)
		{
			uint32_t resID = glsl.get_decoration(resource.id, spv::DecorationLocation);
			glsl.set_name(resource.id, inputName + std::to_string(resID));
		}

		// Compile to GLSL, ready to give to GL driver.
		glsl.build_combined_image_samplers();
		spirv_cross_util::inherit_combined_sampler_bindings(glsl);
		std::string source = glsl.compile();

		// remove all the uniform buffer objects
		std::stringstream ss(source);
		std::string line;
		source = "";
		bool inUBO = false;
		std::vector<std::string> uboNames;
		while (std::getline(ss, line)) {
			if (line.find("layout(binding") != std::string::npos &&
				line.find("uniform") != std::string::npos &&
				line.find("sampler2D") == std::string::npos &&
				line.find("samplerCube") == std::string::npos) // i know, ewww
			{
				inUBO = true;
				continue;
			}
			else if (inUBO){
				if (line == "{")
					continue;
				else if (line[0] == '}') {
					inUBO = false;
					uboNames.push_back(line.substr(2, line.size() - 3));
					continue;
				} else {
					size_t playout = line.find(")");
					if (playout != std::string::npos)
						line.erase(0, playout+2);
					line = "uniform " + line;
				}
			}
			else { // remove all occurances of "ubo." substrings
				for (int i = 0; i < uboNames.size(); i++) {
					std::string what = uboNames[i] + ".";
					size_t n = what.length();
					for (size_t j = line.find(what); j != std::string::npos; j = line.find(what))
						line.erase(j, n);
				}
			}

			source += line + "\n";
		}

		ed::Logger::Get().Log("Finished transcompiling a HLSL shader");

		return source;
	}
	bool HLSL2GLSL::IsHLSL(const std::string& file)
	{
		std::vector<std::string>& exts = Settings::Instance().General.HLSLExtensions;
		return std::count(exts.begin(), exts.end(), file.substr(file.find_last_of('.') + 1)) > 0;
	}
}
