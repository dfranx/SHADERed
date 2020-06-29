#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <SDL2/SDL_events.h>
#include <SHADERed/Objects/PluginAPI/Plugin.h>

namespace ed {
	class KeyboardShortcuts {
	public:
		static KeyboardShortcuts& Instance()
		{
			static KeyboardShortcuts ret;
			return ret;
		}
		struct Shortcut {
			int Key1;
			int Key2;
			bool Alt;
			bool Ctrl;
			bool Shift;
			std::function<void()> Function;
			IPlugin1* Plugin;

			Shortcut()
					: Key1(-1)
					, Key2(-1)
					, Alt(false)
					, Ctrl(false)
					, Shift(false)
					, Function(nullptr)
					, Plugin(nullptr)
			{
			}
			Shortcut(int k1, int k2, bool alt, bool ctrl, bool shift)
					: Key1(k1)
					, Key2(k2)
					, Alt(alt)
					, Ctrl(ctrl)
					, Shift(shift)
					, Plugin(nullptr)
			{
			}
		};

		KeyboardShortcuts();

		void Load();
		void Save();

		// set vk2 to negative if not used
		std::string Exists(const std::string& name, int VK1, int VK2, bool alt, bool ctrl, bool shift);
		bool Set(const std::string& name, int VK1, int VK2, bool alt, bool ctrl, bool shift);
		inline void Remove(const std::string& name)
		{
			m_data[name].Alt = m_data[name].Ctrl = m_data[name].Shift = false;
			m_data[name].Key1 = m_data[name].Key2 = -1;
		}
		void SetCallback(const std::string& name, std::function<void()> func);
		void RegisterPluginShortcut(IPlugin1* plugin, const std::string& name);

		std::string GetString(const std::string& name);
		std::vector<std::string> GetNameList();

		inline std::map<std::string, Shortcut> GetMap() { return m_data; }
		inline void SetMap(std::map<std::string, Shortcut>& m) { m_data = m; }

		inline void Detach(const std::string& name) { m_data.erase(name); }

		void Check(const SDL_Event& e, bool codeHasFocus);

	private:
		bool m_canSolo(const std::string& name, int k);

		int m_keys[2];
		std::map<std::string, Shortcut> m_data;
	};
}