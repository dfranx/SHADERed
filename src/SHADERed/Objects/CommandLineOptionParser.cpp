#include <SHADERed/Objects/CommandLineOptionParser.h>
#include <SHADERed/Objects/WebAPI.h>
#include <string.h>
#include <filesystem>
#include <vector>

namespace ed {
	CommandLineOptionParser::CommandLineOptionParser()
	{
		Fullscreen = false;
		Maximized = false;
		MinimalMode = false;
		PerformanceMode = false;
		LaunchUI = true;
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
			// --version, -v
			else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
				printf("Version %s\n", WebAPI::Version);
				printf("Internal version: %d\n", WebAPI::InternalVersion);

				LaunchUI = false;
			}
			// --help, -h
			else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
				static const std::vector<std::pair<std::string, std::string>> opts = {
					{ "--help | -h", "print this message" },
					{ "--version | -v", "print SHADERed version" },
					{ "--minimal | -m", "launch SHADERed with no UI" },
					{ "--wwidth | -ww [width]", "set window width" },
					{ "--wheight | -wh [height]", "set window height" },
					{ "--fullscreen | -fs", "launch SHADERed in fullscreen mode" },
					{ "--maxmimized | -max", "maximize SHADERed's window" },
					{ "--performance | -p", "launch SHADERed in performance mode" },
				};

				int maxSize = 0;
				for (const auto& opt : opts)
					maxSize = std::max<int>(opt.first.size(), maxSize);

				for (const auto& opt : opts)
					printf("\t%s%*s - %s\n", opt.first.c_str(), maxSize - opt.first.size(), "", opt.second.c_str());

				LaunchUI = false;
			}
			// file path
			else if (std::filesystem::exists(cmdDir / argv[i]))
				ProjectFile = (cmdDir / argv[i]).generic_string();
			// invalid command
			else
				printf("Invalid option '%s'\n", argv[i]);
		}
	}
}