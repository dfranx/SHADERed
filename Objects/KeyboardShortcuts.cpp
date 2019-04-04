#include "KeyboardShortcuts.h"
#include <Windows.h>
#include <fstream>
#include <sstream>

namespace ed
{
	KeyboardShortcuts::KeyboardShortcuts()
	{
		m_keys[0] = m_keys[1] = -1;
	}
	void KeyboardShortcuts::Load()
	{
		std::ifstream file("shortcuts.kb");
		std::string str;

		while (std::getline(file, str)) {
			std::stringstream ss(str);
			std::string name, token;

			ss >> name;

			if (name.empty()) continue;

			m_data[name].Key1 = -1;
			m_data[name].Key2 = -1;

			while (ss >> token)
			{
				if (token == "CTRL")
					m_data[name].Ctrl = true;
				else if (token == "ALT")
					m_data[name].Alt = true;
				else if (token == "SHIFT")
					m_data[name].Shift = true;
				else {
					for (int i = 0; i < 0xE8; i++) {
						if (token == ml::Keyboard::KeyToString(i)) {
							if (m_data[name].Key1 == -1)
								m_data[name].Key1 = i;
							else if (m_data[name].Key2 == -1)
								m_data[name].Key2 = i;
						}
					}
				}				
			}
		}

		return;
	}
	void KeyboardShortcuts::Save()
	{
		std::ofstream file("shortcuts.kb");
		std::string str;

		for (auto& s : m_data) {
			if (s.second.Key1 == -1)
				continue;

			file << s.first;

			if (s.second.Ctrl)
				file << " CTRL";
			if (s.second.Alt)
				file << " ALT";
			if (s.second.Shift)
				file << " SHIFT";
			file << " " << ml::Keyboard::KeyToString(s.second.Key1);
			if (s.second.Key2 != -1)
				file << " " << ml::Keyboard::KeyToString(s.second.Key2);

			file << std::endl;
		}

		return;
	}
	bool KeyboardShortcuts::Set(const std::string& name, int VK1, int VK2, bool alt, bool ctrl, bool shift)
	{
		if (VK1 == -1 || (alt == false && ctrl == false && shift == false && !m_canSolo(VK1)))
			return false;
		
		for (auto& i : m_data)
			if (i.second.Ctrl == ctrl && i.second.Alt == alt && i.second.Shift == shift && i.second.Key1 == VK1 && (VK2 == -1 || i.second.Key2 == VK2 || i.second.Key2 == -1)) {
				i.second.Ctrl = i.second.Alt = i.second.Shift = false;
				i.second.Key1 = i.second.Key2 = -1;
			}
		m_data[name].Alt = alt;
		m_data[name].Ctrl = ctrl;
		m_data[name].Shift = shift;
		m_data[name].Key1 = VK1;
		m_data[name].Key2 = VK2;

		return true;
	}
	void KeyboardShortcuts::SetCallback(const std::string& name, std::function<void()> func)
	{
		m_data[name].Function = func;
	}
	std::string KeyboardShortcuts::GetString(const std::string& name)
	{
		if (m_data[name].Key1 == -1 || (m_data[name].Key1 == 0 && m_data[name].Key2 == 0))
			return "";

		std::string ret = "";

		if (m_data[name].Ctrl)
			ret += "CTRL+";
		if (m_data[name].Alt)
			ret += "ALT+";
		if (m_data[name].Shift)
			ret += "SHIFT+";
		ret += ml::Keyboard::KeyToString(m_data[name].Key1) + "+";
		if (m_data[name].Key2 != -1)
			ret += ml::Keyboard::KeyToString(m_data[name].Key2) + "+";

		return ret.substr(0, ret.size() - 1);
	}
	std::vector<std::string> KeyboardShortcuts::GetNameList()
	{
		std::vector<std::string> ret;
		for (auto i : m_data)
			ret.push_back(i.first);
		return ret;
	}
	void KeyboardShortcuts::Check(const ml::Event& e)
	{
		m_keys[0] = m_keys[1];
		m_keys[1] = e.Keyboard.VK;

		for (auto hotkey : m_data) {
			Shortcut s = hotkey.second;
			if (s.Alt == e.Keyboard.Alt && s.Ctrl == e.Keyboard.Control && s.Shift == e.Keyboard.Shift) {
				int key2 = m_keys[1];
				if (s.Key2 == -1 && s.Key1 == key2 && s.Function != nullptr) {

					/*
					 [ 'G', 'S' ] -> it would call the CTRL+S instead of CTRL+G+S shortcut.
					 That is why we check if we actually meant CTRL+S or CTRL+G+S
					*/
					bool found = false;
					int key1 = m_keys[0];
					if (key1 != -1)
						for (auto clone : m_data)
							if (clone.second.Alt == e.Keyboard.Alt && clone.second.Ctrl == e.Keyboard.Control && clone.second.Shift == e.Keyboard.Shift &&
								clone.second.Key1 == key1 && clone.second.Key2 == key2)
							{
								found = true;
							}

					// call the proper function
					if (!found) {
						s.Function();
						m_keys[1] = -1;
					}
				}
				else if (s.Key2 != -1) {
					if (m_keys[0] != -1) {
						int key1 = m_keys[0];

						if (s.Key1 == key1 && s.Key2 == key2 && s.Function != nullptr) {
							s.Function();

							m_keys[1] = -1;
							m_keys[0] = -1;
						}
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