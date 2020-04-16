#include <SHADERed/Objects/CommandLineOptionParser.h>
#include <filesystem>

namespace ed {
	CommandLineOptionParser::CommandLineOptionParser()
	{
		Fullscreen = false;
		Maximized = false;
		MinimalMode = false;
		PerformanceMode = false;
		ProjectFile = "";
		WindowWidth = WindowHeight = 0;
	}
	void CommandLineOptionParser::Parse(const std::filesystem::path& cmdDir, int argc, char* argv[])
	{
		for (int i = 0; i < argc; i++) {
			// --minimal, -m
			if (strcmp(argv[i], "--minimal") == 0 || strcmp(argv[i], "-m") == 0) {
				MinimalMode = true;
			}
			// --wwidth, -ww [width]
			else if (strcmp(argv[i], "--wwidth") == 0 || strcmp(argv[i], "-ww") == 0) {
				int width = 0;
				if (i + 1 < argc) {
					width = atoi(argv[i + 1]);
					i++;
				}
				WindowWidth = width;
			}
			// --wheight, -wh [height]
			else if (strcmp(argv[i], "--wheight") == 0 || strcmp(argv[i], "-wh") == 0) {
				int height = 0;
				if (i + 1 < argc) {
					height = atoi(argv[i + 1]);
					i++;
				}
				WindowHeight = height;
			}
			// --fullscreen, -fs
			else if (strcmp(argv[i], "--fullscreen") == 0 || strcmp(argv[i], "-fs") == 0) {
				Fullscreen = true;
			}
			// --maximized, -max
			else if (strcmp(argv[i], "--maxmimized") == 0 || strcmp(argv[i], "-max") == 0) {
				Maximized = true;
			}
			// --performance, -p
			else if (strcmp(argv[i], "--performance") == 0 || strcmp(argv[i], "-p") == 0) {
				PerformanceMode = true;
			}
			// file path
			else if (std::filesystem::exists(cmdDir / argv[i]))
				ProjectFile = (cmdDir / argv[i]).generic_string();
		}
	}
}