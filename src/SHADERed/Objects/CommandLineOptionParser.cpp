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

		Render = false;
		RenderSequence = false;
		RenderWidth = 1920;
		RenderHeight = 1080;
		RenderSupersampling = 1;
		RenderTime = 0.0f;
		RenderPath = "render.png";
		RenderFrameIndex = 0;
		RenderSequenceFPS = 30;
		RenderSequenceDuration = 0.5f;
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
			// --render, -r
			else if (strcmp(argv[i], "--render") == 0 || strcmp(argv[i], "-r") == 0) {
				Render = true;

				if (i + 1 < argc) {
					RenderPath = (cmdDir / argv[i + 1]).generic_string();
					i++;
				}
			}
			// --rendersequence, -rseq
			else if (strcmp(argv[i], "--rendersequence") == 0 || strcmp(argv[i], "-rseq") == 0) {
				Render = true;
				RenderSequence = true;
			}
			// --renderwidth, -rw [width]
			else if (strcmp(argv[i], "--renderwidth") == 0 || strcmp(argv[i], "-rw") == 0) {
				int width = 0;
				if (i + 1 < argc) {
					width = atoi(argv[i + 1]);
					i++;
				}
				RenderWidth = width;
			}
			// --renderheight, -rh [height]
			else if (strcmp(argv[i], "--renderheight") == 0 || strcmp(argv[i], "-rh") == 0) {
				int height = 0;
				if (i + 1 < argc) {
					height = atoi(argv[i + 1]);
					i++;
				}
				RenderHeight = height;
			}
			// --rendersample, -rsmp [sample_count]
			else if (strcmp(argv[i], "--rendersample") == 0 || strcmp(argv[i], "-rsmp") == 0) {
				int smp = 0;
				if (i + 1 < argc) {
					smp = atoi(argv[i + 1]);
					i++;
				}
				if (smp == 2)
					RenderSupersampling = 2;
				else if (smp == 4)
					RenderSupersampling = 4;
				else if (smp == 8)
					RenderSupersampling = 8;
				else {
					if (smp != 1)
						printf("The argument for number of samples is invalid - it can only be 1, 2, 4 or 8\n");
					RenderSupersampling = 1;
				}
			}
			// --renderframe, -rf [index]
			else if (strcmp(argv[i], "--renderframe") == 0 || strcmp(argv[i], "-rf") == 0) {
				int frame = 0;
				if (i + 1 < argc) {
					frame = atoi(argv[i + 1]);
					i++;
				}
				RenderFrameIndex = std::max<int>(0, frame);
			}
			// --renderseqfps, -rseqfps [index]
			else if (strcmp(argv[i], "--renderseqfps") == 0 || strcmp(argv[i], "-rseqfps") == 0) {
				int fps = 0;
				if (i + 1 < argc) {
					fps = atoi(argv[i + 1]);
					i++;
				}
				RenderSequenceFPS = std::max<int>(1, fps);
			}
			// --renderseqduration, -rseqdur [time]
			else if (strcmp(argv[i], "--renderseqduration") == 0 || strcmp(argv[i], "-rseqdur") == 0) {
				float dur = 0;
				if (i + 1 < argc) {
					dur = atof(argv[i + 1]);
					i++;
				}
				RenderSequenceDuration = std::max<float>(0.0f, dur);
			}
			// --rendertime, -rt [time]
			else if (strcmp(argv[i], "--rendertime") == 0 || strcmp(argv[i], "-rt") == 0) {
				float tm = 0;
				if (i + 1 < argc) {
					tm = atof(argv[i + 1]);
					i++;
				}
				RenderTime = std::max<float>(0.0f, tm);
			}
			// --help, -h
			else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
				static const std::vector<std::pair<std::string, std::string>> opts = {
					{ "--help | -h", "print this message" },
					{ "--version | -v", "print SHADERed version" },
					{ "--minimal | -m", "launch SHADERed with no UI" },
					{ "--wwidth | -ww <width>", "set window width" },
					{ "--wheight | -wh <height>", "set window height" },
					{ "--fullscreen | -fs", "launch SHADERed in fullscreen mode" },
					{ "--maxmimized | -max", "maximize SHADERed's window" },
					{ "--performance | -p", "launch SHADERed in performance mode" },
					{ "--render | -r <file>", "render to a file" },
					{ "--renderwidth | -rw <width>", "set the output image width" },
					{ "--renderheight | -rh <height>", "set the output image height" },
					{ "--rendersample | -rsmp <samples>", "set the rendering supersample (1, 2, 4 or 8)" },
					{ "--renderframe | -rf <index>", "set the FrameIndex system variable" },
					{ "--rendertime | -rt <time>", "set the Time system variable" },
					{ "--rendersequence | -rseq", "render a sequence" },
					{ "--renderseqfps | -rseqfps <index>", "set sequence FPS" },
					{ "--renderseqduration | -rseqdur <time>", "set sequence duration" },
					{ "<file>", "open a file" }
				};

				int maxSize = 0;
				for (const auto& opt : opts)
					maxSize = std::max<int>(opt.first.size(), maxSize);

				printf("List of commands:\n");
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