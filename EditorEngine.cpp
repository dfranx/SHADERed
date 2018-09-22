#include "EditorEngine.h"

namespace ed
{
	EditorEngine::EditorEngine(ml::Window* wnd) :
		m_ui(&m_interface, wnd),
		m_interface(wnd)
	{
	}
	void EditorEngine::Create()
	{
		m_ui.LoadSettings();
	}
	void EditorEngine::OnEvent(const ml::Event & e)
	{
		m_ui.OnEvent(e);
		m_interface.OnEvent(e);
	}
	void EditorEngine::Update(float delta)
	{
		m_ui.Update(delta);
		m_interface.Update(delta);
	}
	void EditorEngine::Render()
	{
		m_ui.Render();
	}
	void EditorEngine::Destroy()
	{
		m_ui.SaveSettings();
	}
}