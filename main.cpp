#include <MoonLight/Base/Window.h>
#include <MoonLight/Base/Event.h>
#include "EditorEngine.h"
#include <fstream>

/*
TODO:
	shader variable functions
	check for value change in InputLayout and VariableContainer - needs memcpy all alocated data
	.sprj -> project "Save" and "Open" functionalities
	save per-window(window and widget sizes, positions, etc...) and per-project(opened files, property viewer item, etc..) settings
	shader "..." button
	output window (outputs errors, warnings and other messages) + error&warning stack (check if project has any errors)
	code editor
	code stats
	create pipeline items
	movable geometry (left click)
	various states -> Blend States, Rasterizer State, Depth-Stencil State, etc...
		or
	add Passes aka groups - merge shaders and all states into it
*/
int main()
{
	// window configuration
	ml::Window::Config wndConfig;
	wndConfig.Fullscreen = false;
	wndConfig.Sample.Count = 1;
	wndConfig.Sample.Quality = 0;

	// load window size
	short wndWidth = 800, wndHeight = 600, wndPosX = -1, wndPosY = -1;
	std::ifstream preload("preload.dat");
	if (preload.is_open()) {
		preload.read(reinterpret_cast<char*>(&wndWidth), 2);
		preload.read(reinterpret_cast<char*>(&wndHeight), 2);
		preload.read(reinterpret_cast<char*>(&wndPosX), 2);
		preload.read(reinterpret_cast<char*>(&wndPosY), 2);
		preload.close();
	}

	// open window
	ml::Window wnd;
	wnd.Create(DirectX::XMINT2(wndWidth, wndHeight), "HLSLed", ml::Window::Style::Resizable, wndConfig);
	if (wndPosX != -1 && wndPosY != -1)
		wnd.SetPosition(DirectX::XMINT2(wndPosX, wndPosY));

	// create engine
	ed::EditorEngine engine(&wnd);
	engine.Create();

	// timer for time delta
	ml::Timer timer;

	ml::Event e;
	while (wnd.IsOpen()) {
		while (wnd.GetEvent(e)) {
			if (e.Type == ml::EventType::WindowResize) {
				// cache window size and position
				wndWidth = wnd.GetSize().x;
				wndHeight = wnd.GetSize().y;
				wndPosX = wnd.GetPosition().x;
				wndPosY = wnd.GetPosition().y;
			}
			engine.OnEvent(e);
		}

		float delta = timer.Restart();
		engine.Update(delta);

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
	std::ofstream save("preload.dat");
	converter.size = wndWidth;				// write window width
	save.write(converter.data, 2);
	converter.size = wndHeight;				// write window height
	save.write(converter.data, 2);
	converter.size = wndPosX;				// write window position x
	save.write(converter.data, 2);
	converter.size = wndPosY;				// write window position y
	save.write(converter.data, 2);

	save.close();

	// close and free the memory
	engine.Destroy();
	wnd.Destroy();

	return 0;
}