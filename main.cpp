#include <MoonLight/Base/Window.h>
#include <MoonLight/Base/Event.h>
#include "EditorEngine.h"

int main()
{
	// window configuration
	ml::Window::Config wndConfig;
	wndConfig.Fullscreen = false;
	wndConfig.Sample.Count = 1;
	wndConfig.Sample.Quality = 0;

	// open window
	ml::Window wnd;
	wnd.Create(DirectX::XMINT2(800, 600), "Test project!", ml::Window::Style::Resizable, wndConfig);
	
	// create engine
	ed::EditorEngine engine(&wnd);
	engine.Create();

	ml::Event e;
	while (wnd.IsOpen()) {
		while (wnd.GetEvent(e)) {
			engine.OnEvent(e);
		}

		engine.Update(0.0f);

		wnd.Clear();
		wnd.ClearDepthStencil(1.0f, 0);

		engine.Render();

		wnd.Render();
	}

	engine.Destroy();
	wnd.Destroy();

	return 0;
}