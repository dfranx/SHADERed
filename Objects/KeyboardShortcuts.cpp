#include "KeyboardShortcuts.h"
#include <Windows.h>

namespace ed
{
	KeyboardShortcuts::KeyboardShortcuts()
	{
		m_keys[0] = m_keys[1] = -1;
	}
	bool KeyboardShortcuts::Attach(const std::string& name, int VK1, int VK2, bool alt, bool ctrl, bool shift, std::function<void()> func)
	{
		if (VK1 == -1 || (alt == false && ctrl == false && shift == false && !m_canSolo(VK1)) || func == nullptr)
			return false;

		m_data[name].Alt = alt;
		m_data[name].Ctrl = ctrl;
		m_data[name].Shift = shift;
		m_data[name].Key1 = VK1;
		m_data[name].Key2 = VK2;
		m_data[name].Function = func;
	}
	void KeyboardShortcuts::Check(const ml::Event& e)
	{
		m_keys[0] = m_keys[1];
		m_keys[1] = e.Keyboard.VK;

		bool shift = GetAsyncKeyState(VK_LSHIFT) || GetAsyncKeyState(VK_RSHIFT);
		bool alt = GetAsyncKeyState(VK_LMENU) || GetAsyncKeyState(VK_RMENU);
		bool ctrl = GetAsyncKeyState(VK_RCONTROL) || GetAsyncKeyState(VK_LCONTROL);

		for (auto hotkey : m_data) {
			Shortcut s = hotkey.second;
			if (s.Alt == e.Keyboard.Alt && s.Ctrl == e.Keyboard.Control && s.Shift == e.Keyboard.Shift) {
				int key2 = m_keys[1];
				if (s.Key2 == -1 && s.Key1 == key2)
					s.Function();
				else if (s.Key2 != -1) {
					if (m_keys[0] != -1) {
						int key1 = m_keys[0];
						if (s.Key1 == key1 && s.Key2 == key2)
							s.Function();
					}
				}
			}
		}
	}
	bool KeyboardShortcuts::m_canSolo(int k)
	{
		return (k >= VK_F1 && k <= VK_F24);
	}
}