#pragma once
#include <string>
#include <filesystem>

namespace ed {
	class CommandLineOptionParser {
	public:
		CommandLineOptionParser();

		void Parse(const std::filesystem::path& cmdDir, int argc, char* argv[]);

		bool LaunchUI;

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
	};
}