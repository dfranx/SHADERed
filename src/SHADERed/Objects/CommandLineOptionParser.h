#pragma once
#include <string>
#include <filesystem>
#include <SHADERed/Objects/ShaderStage.h>
#include <SHADERed/Objects/ShaderLanguage.h>

namespace ed {
	class CommandLineOptionParser {
	public:
		CommandLineOptionParser();

		void Parse(const std::filesystem::path& cmdDir, int argc, char* argv[]);

		void Execute();

		bool LaunchUI;

		std::string CompilePath, CompileOutput, CompileEntry;
		ed::ShaderStage CompileStage;
		ed::ShaderLanguage CompileLanguage;
		bool CompileSPIRV;

		std::string ConvertPath;

		std::string RenderPath;
		bool Render, RenderSequence;
		int RenderWidth, RenderHeight, RenderSupersampling, RenderFrameIndex, RenderSequenceFPS;
		float RenderTime, RenderSequenceDuration;

		bool Fullscreen;
		bool Maximized;
		bool PerformanceMode;
		int WindowWidth, WindowHeight;
		bool MinimalMode;
		std::string ProjectFile;

		bool StartDAPServer;

		bool ConvertCPP;
		std::string CMakePath;
	};
}