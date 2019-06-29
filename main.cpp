#include <MoonLight/Base/Window.h>
#include <MoonLight/Base/Event.h>
#include "Objects/Logger.h"
#include "EditorEngine.h"
#include <fstream>

#ifdef NDEBUG
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

HICON hWindowIcon = NULL;
HICON hWindowIconBig = NULL;
void SetIcon(HWND hwnd, std::string stricon)
{
	if (hWindowIcon != NULL)
		DestroyIcon(hWindowIcon);
	if (hWindowIconBig != NULL)
		DestroyIcon(hWindowIconBig);
	if (stricon == "")
	{
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)NULL);
		SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)NULL);
	}
	else
	{
		hWindowIcon = (HICON)LoadImageA(NULL, stricon.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
		hWindowIconBig = (HICON)LoadImageA(NULL, stricon.c_str(), IMAGE_ICON, 256, 256, LR_LOADFROMFILE);
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hWindowIcon);
		SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hWindowIconBig);
	}

}

int main()
{
	// window configuration
	ml::Window::Config wndConfig;
	wndConfig.Fullscreen = false;
	wndConfig.Sample.Count = 1;
	wndConfig.Sample.Quality = 0;

	// load window size
	short wndWidth = 800, wndHeight = 600, wndPosX = -1, wndPosY = -1;
	BOOL fullscreen = FALSE, maximized = FALSE;
	std::ifstream preload("data/preload.dat");
	if (preload.is_open()) {
		preload.read(reinterpret_cast<char*>(&wndWidth), 2);
		preload.read(reinterpret_cast<char*>(&wndHeight), 2);
		preload.read(reinterpret_cast<char*>(&wndPosX), 2);
		preload.read(reinterpret_cast<char*>(&wndPosY), 2);
		fullscreen = preload.get();
		maximized = preload.get();
		preload.close();

		if (wndWidth > GetSystemMetrics(SM_CXVIRTUALSCREEN))
			wndWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		if (wndHeight > GetSystemMetrics(SM_CYVIRTUALSCREEN))
			wndHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	}
	else
		DeleteFileA("data/workspace.dat"); // prevent from crashing

	// open window
	ml::Window wnd;
	wnd.Create(DirectX::XMINT2(wndWidth, wndHeight), "SHADERed", ml::Window::Style::Resizable, wndConfig);
	if (wndPosX != -1 && wndPosY != -1)
		wnd.SetPosition(DirectX::XMINT2(wndPosX, wndPosY));

	SetIcon(wnd.GetWindowHandle(), "icon.ico");

	if (maximized)
		SendMessage(wnd.GetWindowHandle(), WM_SYSCOMMAND, SC_MAXIMIZE, 0);
	if (fullscreen)
		wnd.GetSwapChain()->SetFullscreenState(true, nullptr);

	ed::Logger* log = new ed::Logger();
	wnd.SetLogger(log);

	// create engine
	ed::EditorEngine engine(&wnd);
	engine.Create();

	log->Stack = &engine.Interface().Messages;

	// timer for time delta
	ml::Timer timer;

	ml::Event e;
	while (wnd.IsOpen()) {
		while (wnd.GetEvent(e)) {
			if (e.Type == ml::EventType::WindowResize) {
				WINDOWPLACEMENT wndPlace = { 0 };
				GetWindowPlacement(wnd.GetWindowHandle(), &wndPlace);
				maximized = (wndPlace.showCmd == SW_SHOWMAXIMIZED);

				// save fullscreen state
				wnd.GetSwapChain()->GetFullscreenState(&fullscreen, nullptr);

				if (!maximized) {
					// cache window size and position
					wndWidth = wnd.GetSize().x;
					wndHeight = wnd.GetSize().y;
					wndPosX = wnd.GetPosition().x;
					wndPosY = wnd.GetPosition().y;
				}
			}
			engine.OnEvent(e);
		}

		float delta = timer.Restart();
		engine.Update(delta);
		if (!wnd.IsOpen())
			break;

		wnd.Bind();
		wnd.Clear();
		wnd.ClearDepthStencil(1.0f, 0);

		engine.Render();

		wnd.Render();
	}

	// union for converting short to bytes
	union
	{
		short size;
		char data[2];
	} converter;

	// save window size
	std::ofstream save("data/preload.dat");
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

	save.close();

	// close and free the memory
	engine.Destroy();
	wnd.Destroy();


	return 0;
}