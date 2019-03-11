#pragma once
#include <functional>
#include <string>
#include <vector>
#include <map>

#include <MoonLight/Base/Event.h>

namespace ed
{
	class KeyboardShortcuts
	{
	public:
		static KeyboardShortcuts& Instance()
		{
			static KeyboardShortcuts ret;
			return ret;
		}

		KeyboardShortcuts();

		// set vk2 to negative if not used
		bool Attach(const std::string& name, int VK1, int VK2, bool alt, bool ctrl, bool shift, std::function<void()> func);

		inline void Detach(const std::string& name) { m_data.erase(name); }

		void Check(const ml::Event& e);

	private:
		bool m_canSolo(int k);

		struct Shortcut
		{
			int Key1;
			int Key2;
			bool Alt;
			bool Ctrl;
			bool Shift;
			std::function<void()> Function;
		};

		int m_keys[2];
		std::map<std::string, Shortcut> m_data;
	};
}