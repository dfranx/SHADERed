#ifdef _WIN32
#include <windows.h>
#endif

#include <SDL2/SDL.h>
#include <SHADERed/EditorEngine.h>
#include <SHADERed/Objects/CommandLineOptionParser.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/Export/ExportCPP.h>
#include <glslang/Public/ShaderLang.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <string>

#include <misc/stb_image.h>
#include <misc/stb_image_write.h>

#if defined(__linux__) || defined(__unix__)
#include <libgen.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#if defined(NDEBUG) && defined(_WIN32)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

// SDL defines main
#undef main

void SetIcon(SDL_Window* wnd);
void SetDpiAware();

int main(int argc, char* argv[])
{
	srand(time(NULL));
	
	std::error_code fsError;
	std::filesystem::path cmdDir = std::filesystem::current_path();

	if (argc > 0) {
		if (std::filesystem::exists(std::filesystem::path(argv[0]).parent_path())) {
			std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path(), fsError);

			ed::Logger::Get().Log("Setting current_path to " + std::filesystem::current_path().generic_string());
		}
	}

	// start glslang process
	bool glslangInit = glslang::InitializeProcess();
	ed::Logger::Get().Log("Initializing glslang...");

	if (glslangInit)
		ed::Logger::Get().Log("Finished glslang initialization");
	else
		ed::Logger::Get().Log("Failed to initialize glslang", true);

	ed::CommandLineOptionParser coptsParser;
	coptsParser.Parse(cmdDir, argc - 1, argv + 1);
	coptsParser.Execute();

	if (!coptsParser.LaunchUI)
		return 0;

#if defined(__linux__) || defined(__unix__)
	bool linuxUseHomeDir = false;

	// currently the only supported argument is a path to set the working directory... dont do this check if user wants to explicitly set the working directory,
	// TODO: if more arguments get added, use different methods to check if working directory is being set explicitly
	{
		char result[PATH_MAX + 1];
		ssize_t readlinkRes = readlink("/proc/self/exe", result, PATH_MAX);
		std::string exePath = "";
		if (readlinkRes > 0) {
			result[readlinkRes] = '\0';
			exePath = std::string(dirname(result));
		}
		
		std::vector<std::string> toCheck = {
			"/../share/SHADERed",
			"/../share/shadered"
			// TODO: maybe more paths here?
		};

		for (const auto& wrkpath : toCheck) {
			if (std::filesystem::exists(exePath + wrkpath, fsError)) {
				linuxUseHomeDir = true;
				std::filesystem::current_path(exePath + wrkpath, fsError);
				ed::Logger::Get().Log("Setting current_path to " + std::filesystem::current_path().generic_string());
				break;
			}
		}

		if (access(exePath.c_str(), W_OK) != 0) 
			linuxUseHomeDir = true;		
	}

	if (linuxUseHomeDir) {
		const char *homedir = getenv("XDG_DATA_HOME");
		std::string homedirSuffix = "";
		if (homedir == NULL) {
			homedir = getenv("HOME");
			homedirSuffix = "/.local/share";
		}
		
		if (homedir != NULL) {
			ed::Settings::Instance().LinuxHomeDirectory = std::string(homedir) + homedirSuffix + "/shadered/";

			if (!std::filesystem::exists(ed::Settings::Instance().LinuxHomeDirectory, fsError))
				std::filesystem::create_directory(ed::Settings::Instance().LinuxHomeDirectory, fsError);
			if (!std::filesystem::exists(ed::Settings::Instance().ConvertPath("data"), fsError))
				std::filesystem::create_directory(ed::Settings::Instance().ConvertPath("data"), fsError);
			if (!std::filesystem::exists(ed::Settings::Instance().ConvertPath("themes"), fsError))
				std::filesystem::create_directory(ed::Settings::Instance().ConvertPath("themes"), fsError);
			if (!std::filesystem::exists(ed::Settings::Instance().ConvertPath("plugins"), fsError))
				std::filesystem::create_directory(ed::Settings::Instance().ConvertPath("plugins"), fsError);
		}
	}
#endif

	// create data directory on startup
	if (!std::filesystem::exists(ed::Settings::Instance().ConvertPath("data/"), fsError))
		std::filesystem::create_directory(ed::Settings::Instance().ConvertPath("data/"), fsError);

	// create temp directory
	if (!std::filesystem::exists(ed::Settings::Instance().ConvertPath("temp/"), fsError))
		std::filesystem::create_directory(ed::Settings::Instance().ConvertPath("temp/"), fsError);

	// delete log.txt on startup
	if (std::filesystem::exists(ed::Settings::Instance().ConvertPath("log.txt"), fsError))
		std::filesystem::remove(ed::Settings::Instance().ConvertPath("log.txt"), fsError);

	// set stb_image flags
	stbi_flip_vertically_on_write(1);
	stbi_set_flip_vertically_on_load(1);

	// init sdl2
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
		ed::Logger::Get().Log("Failed to initialize SDL2", true);
		ed::Logger::Get().Save();
		return 0;
	} else
		ed::Logger::Get().Log("Initialized SDL2");

	// load window size
	std::string preloadDatPath = "data/preload.dat";
	if (!ed::Settings::Instance().LinuxHomeDirectory.empty() && std::filesystem::exists(ed::Settings::Instance().LinuxHomeDirectory + preloadDatPath, fsError))
		preloadDatPath = ed::Settings::Instance().ConvertPath(preloadDatPath);
	short wndWidth = 800, wndHeight = 600, wndPosX = -1, wndPosY = -1;
	bool fullscreen = false, maximized = false, perfMode = false;
	std::ifstream preload(preloadDatPath);
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
	} else {
		ed::Logger::Get().Log("File data/preload.dat doesnt exist", true);
		ed::Logger::Get().Log("Deleting data/workspace.dat", true);

		std::filesystem::remove("./data/workspace.dat", fsError);
	}

	// apply parsed CL options
	if (coptsParser.MinimalMode)
		maximized = false;
	if (coptsParser.WindowWidth > 0)
		wndWidth = coptsParser.WindowWidth;
	if (coptsParser.WindowHeight > 0)
		wndHeight = coptsParser.WindowHeight;
	perfMode = perfMode || coptsParser.PerformanceMode;
	fullscreen = fullscreen || coptsParser.Fullscreen;
	maximized = maximized || coptsParser.Maximized;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); // double buffering

	Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

	bool run = true; // should we enter the infinite loop?
	// make the window invisible if only rendering to a file
	if (coptsParser.Render || coptsParser.ConvertCPP) {
		windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;
		maximized = false;
		fullscreen = false;
		run = false;
	}

	// open window
	SDL_Window* wnd = SDL_CreateWindow("SHADERed", (wndPosX == -1) ? SDL_WINDOWPOS_CENTERED : wndPosX, (wndPosY == -1) ? SDL_WINDOWPOS_CENTERED : wndPosY, wndWidth, wndHeight, windowFlags);
	SetDpiAware();
	SDL_SetWindowMinimumSize(wnd, 200, 200);

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
	engine.Interface().Run = run;

	// set window icon:
	SetIcon(wnd);

	engine.UI().SetCommandLineOptions(coptsParser);
	engine.UI().SetPerformanceMode(perfMode);
	engine.Interface().Renderer.AllowComputeShaders(GLEW_ARB_compute_shader);
	engine.Interface().Renderer.AllowTessellationShaders(GLEW_ARB_tessellation_shader);

	// check for filesystem errors
	if (fsError)
		ed::Logger::Get().Log("A filesystem error has occured: " + fsError.message(), true);

	// loop through all OpenGL errors
	GLenum oglError;
	while ((oglError = glGetError()) != GL_NO_ERROR)
		ed::Logger::Get().Log("GL error: " + std::to_string(oglError), true);

	// convert .sprj -> cmake/c++
	if (coptsParser.ConvertCPP) {
		engine.UI().Open(coptsParser.ProjectFile);
		ed::ExportCPP::Export(&engine.Interface(), coptsParser.CMakePath, true, true, "ShaderProject", true, true, true);
		// return 0;
	}

	// render to file
	if (coptsParser.Render) {
		engine.UI().Open(coptsParser.ProjectFile);
		printf("Rendering to file...\n");
		engine.UI().SavePreviewToFile();
	}

	// start the DAP server
	if (coptsParser.StartDAPServer)
		engine.Interface().DAP.Initialize();

	// timer for time delta
	ed::eng::Timer timer;
	SDL_Event event;
	bool minimized = false;
	bool hasFocus = true;
	while (engine.Interface().Run) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				bool cont = true;
				if (engine.Interface().Parser.IsProjectModified()) {
					int btnID = engine.UI().AreYouSure();
					if (btnID == 2)
						cont = false;
				}

				if (cont) {
					engine.Interface().Run = false;
					ed::Logger::Get().Log("Received SDL_QUIT event -> quitting");
				}
			} else if (event.type == SDL_WINDOWEVENT) {
				if (event.window.event == SDL_WINDOWEVENT_MOVED || event.window.event == SDL_WINDOWEVENT_MAXIMIZED || event.window.event == SDL_WINDOWEVENT_RESIZED) {
					Uint32 wndFlags = SDL_GetWindowFlags(wnd);

					maximized = wndFlags & SDL_WINDOW_MAXIMIZED;
					fullscreen = wndFlags & SDL_WINDOW_FULLSCREEN_DESKTOP;
					minimized = false;

					// cache window size and position
					if (!maximized) {
						int tempX = 0, tempY = 0;
						SDL_GetWindowPosition(wnd, &tempX, &tempY);
						wndPosX = tempX;
						wndPosY = tempY;

						SDL_GetWindowSize(wnd, &tempX, &tempY);
						wndWidth = tempX;
						wndHeight = tempY;
					}
				} else if (event.window.event == SDL_WINDOWEVENT_MINIMIZED)
					minimized = true;
				else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
					hasFocus = false;
				else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
					hasFocus = true;
			}
#if defined(_WIN32)
			else if (event.type == SDL_SYSWMEVENT) {
				// this doesn't work - it seems that SDL doesn't forward WM_DPICHANGED message
				if (event.syswm.type == WM_DPICHANGED && ed::Settings::Instance().General.AutoScale) {
					float dpi = 0.0f;
					int wndDisplayIndex = SDL_GetWindowDisplayIndex(wnd);
					SDL_GetDisplayDPI(wndDisplayIndex, &dpi, NULL, NULL);

					if (dpi <= 0.0f) dpi = 1.0f;

					ed::Settings::Instance().TempScale = dpi / 96.0f;
					ed::Logger::Get().Log("Updating DPI to " + std::to_string(dpi / 96.0f));
				}
			}
#endif
			engine.OnEvent(event);
		}

		if (!engine.Interface().Run) break;

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

	engine.UI().Destroy();

	// union for converting short to bytes
	union {
		short size;
		char data[2];
	} converter;

	// save window size
	preloadDatPath = ed::Settings::Instance().ConvertPath("data/preload.dat");
	if (!coptsParser.Render && !coptsParser.ConvertCPP) {
		ed::Logger::Get().Log("Saving window information");

		std::ofstream save(preloadDatPath);
		converter.size = wndWidth; // write window width
		save.write(converter.data, 2);
		converter.size = wndHeight; // write window height
		save.write(converter.data, 2);
		converter.size = wndPosX; // write window position x
		save.write(converter.data, 2);
		converter.size = wndPosY; // write window position y
		save.write(converter.data, 2);
		save.put(fullscreen);
		save.put(maximized);
		save.put(engine.UI().IsPerformanceMode());
		save.write(converter.data, 2);
		save.close();
	}

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

void SetIcon(SDL_Window* wnd)
{
	float dpi = 0.0f;
	int wndDisplayIndex = SDL_GetWindowDisplayIndex(wnd);
	SDL_GetDisplayDPI(wndDisplayIndex, &dpi, NULL, NULL);
	dpi /= 96.0f;

	if (dpi <= 0.0f) dpi = 1.0f;

	stbi_set_flip_vertically_on_load(0);

	int req_format = STBI_rgb_alpha;
	int width, height, orig_format;
	unsigned char* data = stbi_load(dpi == 1.0f ? "./icon_64x64.png" : "./icon_256x256.png", &width, &height, &orig_format, req_format);
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
	} else { // STBI_rgb_alpha (RGBA)
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
void SetDpiAware()
{
#if defined(_WIN32)
	enum DpiAwareness {
		Unaware,
		System,
		PerMonitor
	};
	typedef HRESULT(WINAPI * SetProcessDpiAwarenessFn)(DpiAwareness);
	typedef BOOL(WINAPI * SetProcessDPIAwareFn)(void);

	HINSTANCE lib = LoadLibraryA("Shcore.dll");

	if (lib) {
		SetProcessDpiAwarenessFn setProcessDpiAwareness = (SetProcessDpiAwarenessFn)GetProcAddress(lib, "SetProcessDpiAwareness");
		if (setProcessDpiAwareness)
			setProcessDpiAwareness(DpiAwareness::PerMonitor);
	} else {
		lib = LoadLibraryA("user32.dll");
		if (lib) {
			SetProcessDPIAwareFn setProcessDPIAware = (SetProcessDPIAwareFn)GetProcAddress(lib, "SetProcessDPIAware");

			if (setProcessDPIAware)
				setProcessDPIAware();
		}
	}

	if (lib)
		FreeLibrary(lib);
#endif
}
