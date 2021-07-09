#include <SHADERed/Objects/CommandLineOptionParser.h>
#include <SHADERed/Objects/WebAPI.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <string.h>
#include <filesystem>
#include <fstream>
#include <vector>

#include <spvgentwo/Logger.h>
#include <spvgentwo/Module.h>
#include <spvgentwo/Grammar.h>
#include <common/HeapAllocator.h>
#include <common/BinaryFileWriter.h>
#include <common/BinaryFileReader.h>
#include <common/ModulePrinter.h>

namespace ed {
	CommandLineOptionParser::CommandLineOptionParser()
	{
		Fullscreen = false;
		Maximized = false;
		MinimalMode = false;
		PerformanceMode = false;
		LaunchUI = true;
		StartDAPServer = false;
		ProjectFile = "";
		WindowWidth = WindowHeight = 0;

		CompileEntry = "";
		CompilePath = CompileOutput = "";
		CompileLanguage = ShaderLanguage::GLSL;
		CompileStage = ShaderStage::Pixel;
		CompileSPIRV = true;

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

		ConvertCPP = false;
		CMakePath = "";
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
			// --compile, -c [file]
			else if (strcmp(argv[i], "--compile") == 0 || strcmp(argv[i], "-c") == 0) {
				CompilePath = "";
				if (i + 1 < argc) {
					CompilePath = argv[i + 1];
					i++;
				}

				LaunchUI = false;
			}
			// --language, -cl [language]
			else if (strcmp(argv[i], "--language") == 0 || strcmp(argv[i], "-cl") == 0) {
				std::string lang = "";
				if (i + 1 < argc) {
					lang = argv[i + 1];
					i++;
				}
				
				std::transform(lang.begin(), lang.end(), lang.begin(), tolower);

				CompileLanguage = ed::ShaderLanguage::GLSL;
				if (lang == "hlsl")
					CompileLanguage = ed::ShaderLanguage::HLSL;
				else if (lang == "vkglsl")
					CompileLanguage = ed::ShaderLanguage::VulkanGLSL;
			}
			// --stage, -cs [stage]
			else if (strcmp(argv[i], "--stage") == 0 || strcmp(argv[i], "-cs") == 0) {
				std::string stage = "";
				if (i + 1 < argc) {
					stage = argv[i + 1];
					i++;
				}

				std::transform(stage.begin(), stage.end(), stage.begin(), tolower);

				CompileStage = ed::ShaderStage::Pixel;
				if (stage == "vert")
					CompileStage = ed::ShaderStage::Vertex;
				else if (stage == "geom")
					CompileStage = ed::ShaderStage::Geometry;
				else if (stage == "comp")
					CompileStage = ed::ShaderStage::Compute;
				else if (stage == "tesc")
					CompileStage = ed::ShaderStage::TessellationControl;
				else if (stage == "tese")
					CompileStage = ed::ShaderStage::TessellationEvaluation;
			}
			// --output, -o [outputpath]
			else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
				CompileOutput = "";
				if (i + 1 < argc) {
					CompileOutput = argv[i + 1];
					i++;
				}
			}
			// --target, -t [spirv|glsl]
			else if (strcmp(argv[i], "--target") == 0 || strcmp(argv[i], "-t") == 0) {
				std::string target = "";
				if (i + 1 < argc) {
					target = argv[i + 1];
					i++;
				}

				std::transform(target.begin(), target.end(), target.begin(), tolower);

				CompileSPIRV = true;
				if (target == "glsl")
					CompileSPIRV = false;
			}
			// --entry, -e [entry]
			else if (strcmp(argv[i], "--entry") == 0 || strcmp(argv[i], "-e") == 0) {
				CompileEntry = "main";
				if (i + 1 < argc) {
					CompileEntry = argv[i + 1];
					i++;
				}
			}
			// --disassemble, -dis [file]
			else if (strcmp(argv[i], "--disassemble") == 0 || strcmp(argv[i], "-dis") == 0) {
				LaunchUI = false;

				std::string disPath = "";
				if (i + 1 < argc) {
					disPath = argv[i + 1];
					i++;

					std::string disassembly;
					bool res = ShaderCompiler::DisassembleSPIRVFromFile(disPath, disassembly, true);
					if (res)
						printf("%s\n", disassembly.c_str());
					else
						printf("Failed to disassemble \"%s\"\n", disPath.c_str());
				}
			}
			// --convert, -con [file]
			else if (strcmp(argv[i], "--convert") == 0 || strcmp(argv[i], "-con") == 0) {
				LaunchUI = false;
				ConvertPath = "";

				if (i + 1 < argc) {
					ConvertPath = argv[i + 1];
					i++;
				}
			}
			// --generatecmake, -gcm [path]
			else if (strcmp(argv[i], "--generatecmake") == 0 || strcmp(argv[i], "-gcm") == 0) {
				ConvertCPP = true;
				CMakePath = "";

				if (i + 1 < argc) {
					CMakePath = argv[i + 1];
					i++;
				}
			}
			// -dap
			else if (strcmp(argv[i], "-dap") == 0)
				StartDAPServer = true;
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

					{ "--compile | -c <file>", "compile a shader file" },
					{ "--language | -cl <language>", "compiler input language" },
					{ "--stage | -cs <stage>", "compiler input stage; stage can be one of these: vert, geom, tesc, tese, frag, comp" },
					{ "--output | -o <path>", "compiler output path" },
					{ "--target | -t <spirv|glsl>", "choose whether to compile to SPIR-V or GLSL" },
					{ "--entry | -e <funcname>", "shader entry" },

					{ "--disassemble | -dis <file>", "disassemble SPIR-V file" },
					{ "--convert | -con <file>", "convert HLSL to GLSL or GLSL to HLSL" },

					{ "--generatecmake | -gcm <path>", "convert SHADERed project to C++/CMake" },

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
	void CommandLineOptionParser::Execute()
	{
		// --compile
		if (!CompilePath.empty()) {
			std::vector<unsigned int> spv;
			std::vector<ed::ShaderMacro> macros;
			bool status = ed::ShaderCompiler::CompileToSPIRV(spv, CompileLanguage, CompilePath, CompileStage, CompileEntry, macros, nullptr, nullptr);
			if (!status) {
				printf("Failed to compile the shader.\n");
			} else {
				if (CompileSPIRV) {
					std::ofstream spvOut(CompileOutput, std::ios::out | std::ios::binary);
					spvOut.write((char*)spv.data(), spv.size() * sizeof(unsigned int));
					spvOut.close();
				} else {
					std::string glslSource = ed::ShaderCompiler::ConvertToGLSL(spv, CompileLanguage, CompileStage, false, false, nullptr);

					std::ofstream glslOut(CompileOutput, std::ios::out | std::ios::binary);
					glslOut.write(glslSource.c_str(), glslSource.size());
					glslOut.close();
				}
				printf("Done compiling.");
			}
		}

		// --convert
		if (!ConvertPath.empty()) {
			if (CompileLanguage == ShaderLanguage::GLSL) {
				std::vector<unsigned int> spv;
				std::vector<ed::ShaderMacro> macros;
				bool status = ed::ShaderCompiler::CompileToSPIRV(spv, ShaderLanguage::GLSL, ConvertPath, CompileStage, CompileEntry, macros, nullptr, nullptr);

				if (status && spv.size() > 0) {
					std::string converted = ed::ShaderCompiler::ConvertToHLSL(spv, CompileStage);
					printf("%s\n", converted.c_str());
				}
			} else {
				std::vector<unsigned int> spv;
				std::vector<ed::ShaderMacro> macros;
				bool status = ed::ShaderCompiler::CompileToSPIRV(spv, ShaderLanguage::HLSL, ConvertPath, CompileStage, CompileEntry, macros, nullptr, nullptr);

				if (status && spv.size() > 0) {
					std::string converted = ed::ShaderCompiler::ConvertToGLSL(spv, ShaderLanguage::HLSL, CompileStage, false, false, nullptr, false);
					printf("%s\n", converted.c_str());
				}
			}
		}
	}
}