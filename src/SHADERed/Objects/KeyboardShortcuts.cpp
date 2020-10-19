#include <SHADERed/Objects/KeyboardShortcuts.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/Names.h>

#include <fstream>
#include <sstream>
#include <filesystem>

#include <ImGuiColorTextEdit/TextEditor.h>
#include <SDL2/SDL_keyboard.h>

namespace ed {
	std::string getShortcutsFilePath()
	{
		std::string path = "data/shortcuts.kb";
		if (!ed::Settings::Instance().LinuxHomeDirectory.empty() && std::filesystem::exists(ed::Settings::Instance().ConvertPath(path)))
			path = ed::Settings::Instance().ConvertPath(path);
		return path;
	}

	KeyboardShortcuts::KeyboardShortcuts()
	{
		m_keys[0] = m_keys[1] = -1;
	}
	void KeyboardShortcuts::Load()
	{
		ed::Logger::Get().Log("Loading keyboard shortcuts");

		std::ifstream file(getShortcutsFilePath());
		std::string str;

		// pre setup Editor shortcuts (TODO: improve this... TextEditor::GetDefaultShortcuts())
		std::vector<TextEditor::Shortcut> eds = TextEditor::GetDefaultShortcuts();
		for (int i = 0; i < eds.size(); i++)
			Set(std::string("Editor." + std::string(EDITOR_SHORTCUT_NAMES[i])).c_str(), eds[i].Key1, eds[i].Key2, eds[i].Alt, eds[i].Ctrl, eds[i].Shift);

		while (std::getline(file, str)) {
			std::stringstream ss(str);
			std::string name, token;

			ss >> name;

			if (name.empty()) continue;

			int vk1 = -1, vk2 = -1;
			bool alt = false, ctrl = false, shift = false;
			while (ss >> token) {
				if (token == "CTRL")
					ctrl = true;
				else if (token == "ALT")
					alt = true;
				else if (token == "SHIFT")
					shift = true;
				else if (token == "NONE")
					break;
				else {
					if (vk1 == -1)
						vk1 = SDL_GetKeyFromName(token.c_str());
					else if (m_data[name].Key2 == -1)
						vk2 = SDL_GetKeyFromName(token.c_str());
				}
			}

			Set(name, vk1, vk2, alt, ctrl, shift);
		}

		ed::Logger::Get().Log("Loaded shortcut information");
	}
	void KeyboardShortcuts::Save()
	{
		ed::Logger::Get().Log("Saving keyboard shortcuts");

		std::ofstream file(ed::Settings::Instance().ConvertPath("data/shortcuts.kb"));
		std::string str;

		for (auto& s : m_data) {
			if (s.second.Key1 == -1) {
				//file << " NONE" << std::endl;
				continue;
			}

			file << s.first;

			if (s.second.Ctrl)
				file << " CTRL";
			if (s.second.Alt)
				file << " ALT";
			if (s.second.Shift)
				file << " SHIFT";
			file << " " << SDL_GetKeyName(s.second.Key1);
			if (s.second.Key2 != -1)
				file << " " << SDL_GetKeyName(s.second.Key2);

			file << std::endl;
		}

		ed::Logger::Get().Log("Saved shortcut information");

		return;
	}
	std::string KeyboardShortcuts::Exists(const std::string& name, int VK1, int VK2, bool alt, bool ctrl, bool shift)
	{
		for (auto& i : m_data)
			if (name != i.first && i.second.Ctrl == ctrl && i.second.Alt == alt && i.second.Shift == shift && i.second.Key1 == VK1 && (VK2 == -1 || i.second.Key2 == VK2 || i.second.Key2 == -1)) {
				if (!(name == "CodeUI.Save" && i.first == "Project.Save") && !(name == "Project.Save" && i.first == "CodeUI.Save") &&
					((name.find("Editor") == std::string::npos && i.first.find("Editor") == std::string::npos) || (name.find("Editor") != std::string::npos && i.first.find("Editor") != std::string::npos && // autocomplete is a "special module" added to the text editor and not actually the text editor
					(name.find("Autocomplete") == std::string::npos && i.first.find("Autocomplete") == std::string::npos))))
				{
					return i.first;
				}
			}
		return "";
	}
	bool KeyboardShortcuts::Set(const std::string& name, int VK1, int VK2, bool alt, bool ctrl, bool shift)
	{
		if (VK1 == -1 || (alt == false && ctrl == false && shift == false && !m_canSolo(name, VK1)))
			return false;

		std::string ext = Exists(name, VK1, VK2, alt, ctrl, shift);
		if (!ext.empty())
			Remove(ext);

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
	void KeyboardShortcuts::RegisterPluginShortcut(IPlugin1* plugin, const std::string& name)
	{
		std::string actualName = "Plugin." + name;
		m_data[actualName].Plugin = plugin;
	}
	std::string KeyboardShortcuts::GetString(const std::string& name)
	{
		if (m_data[name].Key1 == -1 || (m_data[name].Key1 == 0 && m_data[name].Key2 == 0))
			return "NONE";

		std::string ret = "";

		if (m_data[name].Ctrl)
			ret += "CTRL+";
		if (m_data[name].Alt)
			ret += "ALT+";
		if (m_data[name].Shift)
			ret += "SHIFT+";
		ret += std::string(SDL_GetKeyName(m_data[name].Key1)) + "+";
		if (m_data[name].Key2 != -1)
			ret += std::string(SDL_GetKeyName(m_data[name].Key2)) + "+";

		return ret.substr(0, ret.size() - 1);
	}
	std::vector<std::string> KeyboardShortcuts::GetNameList()
	{
		std::vector<std::string> ret;
		for (const auto& i : m_data)
			ret.push_back(i.first);
		return ret;
	}
	void KeyboardShortcuts::Check(const SDL_Event& e, bool codeHasFocus)
	{
		// dont process key repeats
		if (e.key.repeat != 0)
			return;

		m_keys[0] = m_keys[1];
		m_keys[1] = e.key.keysym.sym;

		bool alt = e.key.keysym.mod & KMOD_ALT;
		bool ctrl = e.key.keysym.mod & KMOD_CTRL;
		bool shift = e.key.keysym.mod & KMOD_SHIFT;

		bool resetSecond = false, resetFirst = false;

		for (const auto& hotkey : m_data) {
			if (codeHasFocus && !(hotkey.first.find("Editor") != std::string::npos || hotkey.first.find("CodeUI") != std::string::npos || hotkey.first.find("Debug") != std::string::npos || hotkey.first == "Project.Save"))
				continue;

			const Shortcut& s = hotkey.second;
			if (s.Alt == alt && s.Ctrl == ctrl && s.Shift == shift) {
				int key2 = m_keys[1];
				if (s.Key2 == -1 && s.Key1 == key2 && (s.Function != nullptr || s.Plugin != nullptr)) {

					/*
					 [ 'G', 'S' ] -> it would call the CTRL+S instead of CTRL+G+S shortcut.
					 That is why we check if we actually meant CTRL+S or CTRL+G+S
					*/
					bool found = false;
					int key1 = m_keys[0];
					if (key1 != -1)
						for (const auto& clone : m_data)
							if (clone.second.Alt == alt && clone.second.Ctrl == ctrl && clone.second.Shift == shift && clone.second.Key1 == key1 && clone.second.Key2 == key2 && clone.second.Key2 != -1) {
								found = true;
							}

					// call the proper function
					if (!found) {
						if (s.Plugin != nullptr) {
							std::string actualName = hotkey.first.substr(hotkey.first.find_first_of('.') + 1);
							s.Plugin->HandleShortcut(actualName.c_str());
						} else s.Function();

						resetSecond = true;
					}
				} else if (s.Key2 != -1) {
					if (m_keys[0] != -1) {
						int key1 = m_keys[0];

						if (s.Key1 == key1 && s.Key2 == key2 && (s.Function != nullptr || s.Plugin != nullptr)) {
							if (s.Plugin != nullptr) {
								std::string actualName = hotkey.first.substr(hotkey.first.find_first_of('.') + 1);
								s.Plugin->HandleShortcut(actualName.c_str());
							} else s.Function();

							resetFirst = resetSecond = true;
						}
					}
				}
			}
		}

		if (resetFirst) m_keys[0] = -1;
		if (resetSecond) m_keys[1] = -1;
	}
	bool KeyboardShortcuts::m_canSolo(const std::string& name, int k)
	{
		bool isEditorSpecial = (name.find("Editor") != std::string::npos) && !((k >= SDLK_0 && k <= SDLK_9) || (k >= SDLK_a && k <= SDLK_z)); // the key can go solo if it's a "special" key
		return (k >= SDLK_F1 && k <= SDLK_F12) || (k >= SDLK_F13 && k <= SDLK_F24) || isEditorSpecial || (name.find("Editor") == std::string::npos);
	}
}