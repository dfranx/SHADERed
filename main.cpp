#ifdef _WIN32
#include <windows.h>
#endif

#include <SDL2/SDL.h>
#include <glslang/Public/ShaderLang.h>
#include "Objects/AudioShaderStream.h"
#include "Objects/Settings.h"
#include "Objects/Logger.h"
#include "EditorEngine.h"
#include "Engine/GeometryFactory.h"

#include <thread>
#include <chrono>
#include <fstream>
#include <ghc/filesystem.hpp>

#include <stb/stb_image_write.h>
#include <stb/stb_image.h>

#if defined(__linux__) || defined(__unix__)
#include <libgen.h>
#endif

#if defined(NDEBUG) && defined(_WIN32)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

// SDL defines main
#undef main

void setIcon(SDL_Window* wnd)
{
	stbi_set_flip_vertically_on_load(0);

	int req_format = STBI_rgb_alpha;
	int width, height, orig_format;
	unsigned char* data = stbi_load("./icon.png", &width, &height, &orig_format, req_format);
	if (data == NULL) {
		ed::Logger::Get().Log("Failed to set window icon", true);
		return;
	}

	int depth, pitch;
	Uint32 pixel_format;
	if (req_format == STBI_rgb) {
		depth = 24;
		pitch = 3 * width; // 3 bytes per pixel * pixels per row
		pixel_format = SDL_PIXELFORMAT_RGB24;
	}
	else { // STBI_rgb_alpha (RGBA)
		depth = 32;
		pitch = 4 * width;
		pixel_format = SDL_PIXELFORMAT_RGBA32;
	}

	SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom((void*)data, width, height,
		depth, pitch, pixel_format);

	if (surf == NULL) {
		ed::Logger::Get().Log("Failed to create icon SDL_Surface", true);
		stbi_image_free(data);
		return;
	}

	SDL_SetWindowIcon(wnd, surf);

	SDL_FreeSurface(surf);
	stbi_image_free(data);

	stbi_set_flip_vertically_on_load(1);
}

#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[])
{
	if (argc > 0) {
		if (ghc::filesystem::exists(ghc::filesystem::path(argv[0]).parent_path())) {
			ghc::filesystem::current_path(ghc::filesystem::path(argv[0]).parent_path());
			
			ed::Logger::Get().Log("Setting current_path to " + ghc::filesystem::current_path().generic_string());
		}
	}

#if defined(__linux__) || defined(__unix__)
	// currently the only supported argument is a path to set the working directory... dont do this check if user wants to explicitly set the working directory,
	// TODO: if more arguments get added, use different methods to check if working directory is being set explicitly
	if (argc <= 1) { 
		char result[PATH_MAX];
		ssize_t readlinkRes = readlink("/proc/self/exe", result, PATH_MAX);
		std::string exePath = "";
		if (readlinkRes != -1)
			exePath = std::string(dirname(result));

		std::vector<std::string> toCheck = { 
			"/../share/SHADERed",
			"/../share/shadered"
			// TODO: maybe more paths here?
		};

		for (const auto& wrkpath : toCheck) {
			if (ghc::filesystem::exists(exePath + wrkpath)) {
				ghc::filesystem::current_path(exePath + wrkpath);
				ed::Logger::Get().Log("Setting current_path to " + ghc::filesystem::current_path().generic_string());
				break;
			}
		}
	}
#endif

	// create data directory on startup
	if (!ghc::filesystem::exists("./data/"))
		ghc::filesystem::create_directory("./data/");

	// delete log.txt on startup
	if (ghc::filesystem::exists("./log.txt")) {
		std::error_code errCode;
		ghc::filesystem::remove("./log.txt", errCode);
	}

	// set stb_image flags
	stbi_flip_vertically_on_write(1);
	stbi_set_flip_vertically_on_load(1);

	// start glslang process
	bool glslangInit = glslang::InitializeProcess();
	ed::Logger::Get().Log("Initializing glslang...");

	if (glslangInit)
		ed::Logger::Get().Log("Finished glslang initialization");
	else
		ed::Logger::Get().Log("Failed to initialize glslang", true);

	
	// init sdl2
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
		ed::Logger::Get().Log("Failed to initialize SDL2", true);
		ed::Logger::Get().Save();
		return 0;
	} else
		ed::Logger::Get().Log("Initialized SDL2");

	// load window size
	short wndWidth = 800, wndHeight = 600, wndPosX = -1, wndPosY = -1;
	bool fullscreen = false, maximized = false, perfMode = false;
	std::ifstream preload("data/preload.dat");
	if (preload.is_open()) {
		ed::Logger::Get().Log("Loading window information from data/preload.dat");

		preload.read(reinterpret_cast<char*>(&wndWidth), 2);
		preload.read(reinterpret_cast<char*>(&wndHeight), 2);
		preload.read(reinterpret_cast<char*>(&wndPosX), 2);
		preload.read(reinterpret_cast<char*>(&wndPosY), 2);
		fullscreen = preload.get();
		maximized = preload.get();
		if (preload.peek() != EOF)
			perfMode = preload.get();
		preload.close();


		// clamp to desktop size
		SDL_DisplayMode desk;
		SDL_GetCurrentDisplayMode(0, &desk);
		if (wndWidth > desk.w)
			wndWidth = desk.w;
		if (wndHeight > desk.h)
			wndHeight = desk.h;
	}
	else {
		ed::Logger::Get().Log("File data/preload.dat doesnt exist", true);
		ed::Logger::Get().Log("Deleting data/workspace.dat", true);

		std::error_code errCode;
		ghc::filesystem::remove("./data/workspace.dat", errCode);
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); // double buffering

	// open window
	SDL_Window* wnd = SDL_CreateWindow("SHADERed", wndPosX == -1 ? SDL_WINDOWPOS_CENTERED : wndPosX, wndPosY == -1 ? SDL_WINDOWPOS_CENTERED : wndPosY, wndWidth, wndHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_SetWindowMinimumSize(wnd, 200, 200);

	// set window icon:
	setIcon(wnd);

	if (maximized)
		SDL_MaximizeWindow(wnd);
	if (fullscreen)
		SDL_SetWindowFullscreen(wnd, SDL_WINDOW_FULLSCREEN_DESKTOP);

	// get GL context
	SDL_GLContext glContext = SDL_GL_CreateContext(wnd);
	SDL_GL_MakeCurrent(wnd, glContext);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);

	// init glew
	glewExperimental = true;
	if (glewInit() != GLEW_OK) {
		ed::Logger::Get().Log("Failed to initialize GLEW", true);
		ed::Logger::Get().Save();
		return 0;
	} else
		ed::Logger::Get().Log("Initialized GLEW");

	// create engine
	ed::EditorEngine engine(wnd, &glContext);
	ed::Logger::Get().Log("Creating EditorEngine...");
	engine.Create();
	ed::Logger::Get().Log("Created EditorEngine");

	// open an item if given in arguments
	if (argc == 2) {
		ed::Logger::Get().Log("Openning a file provided through argument " + std::string(argv[1]));
		engine.UI().Open(argv[1]);
	}

	engine.UI().SetPerformanceMode(perfMode);
	engine.Interface().Renderer.AllowComputeShaders(GLEW_ARB_compute_shader);

	// timer for time delta
	ed::eng::Timer timer;
	SDL_Event event;
	bool run = true;
	bool minimized = true;
	bool hasFocus = true;
	while (run) {
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT) {
				bool cont = true;
				if (engine.Interface().Parser.IsProjectModified()) {
					int btnID = engine.UI().AreYouSure();
					if (btnID == 2)
						cont = false;
				}

				if (cont) {
					run = false;
					ed::Logger::Get().Log("Received SDL_QUIT event -> quitting");
				}
			}
			else if (event.type == SDL_WINDOWEVENT) {
				if (event.window.event == SDL_WINDOWEVENT_MOVED ||
					event.window.event == SDL_WINDOWEVENT_MAXIMIZED ||
					event.window.event == SDL_WINDOWEVENT_RESIZED) {
					Uint32 wndFlags = SDL_GetWindowFlags(wnd);

					maximized = wndFlags & SDL_WINDOW_MAXIMIZED;
					fullscreen = wndFlags & SDL_WINDOW_FULLSCREEN_DESKTOP;
					minimized = false;

					// cache window size and position
					if (!maximized) {
						int tempX = 0, tempY = 0;
						SDL_GetWindowPosition(wnd, &tempX, &tempY);
						wndPosX = tempX; wndPosY = tempY;

						SDL_GetWindowSize(wnd, &tempX, &tempY);
						wndWidth = tempX; wndHeight = tempY;
					}
				}
				else if (event.window.event == SDL_WINDOWEVENT_MINIMIZED)
					minimized = true;
				else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
					hasFocus = false;
				else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
					hasFocus = true;
			}
			engine.OnEvent(event);
		}

		if (!run) break;

		float delta = timer.Restart();
		engine.Update(delta);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		engine.Render();

		SDL_GL_SwapWindow(wnd);

		if (minimized && delta * 1000 < 33)
			std::this_thread::sleep_for(std::chrono::milliseconds(33 - (int)(delta * 1000)));
		else if (!hasFocus && ed::Settings::Instance().Preview.LostFocusLimitFPS && delta * 1000 < 16)
			std::this_thread::sleep_for(std::chrono::milliseconds(16 - (int)(delta * 1000)));
	}

	// union for converting short to bytes
	union
	{
		short size;
		char data[2];
	} converter;

	// save window size
	std::ofstream save("data/preload.dat");
	

	ed::Logger::Get().Log("Saving window information");

	converter.size = wndWidth;				// write window width
	save.write(converter.data, 2);
	converter.size = wndHeight;				// write window height
	save.write(converter.data, 2);
	converter.size = wndPosX;				// write window position x
	save.write(converter.data, 2);
	converter.size = wndPosY;				// write window position y
	save.write(converter.data, 2);
	save.put(fullscreen);
	save.put(maximized);
	save.put(engine.UI().IsPerformanceMode());

	save.close();

	// close and free the memory
	engine.Destroy();

	// sdl2
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(wnd);
	SDL_Quit();

	ed::Logger::Get().Log("Destroyed EditorEngine and SDL2");

	ed::Logger::Get().Save();

	return 0;
}