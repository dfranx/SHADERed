#pragma once
#include <SHADERed/Objects/PluginAPI/Plugin.h>
#include <SHADERed/Engine/Timer.h>

#include <functional>
#include <vector>
#include <string>

namespace ed {
	class NotificationSystem {
	public:
		void Add(int id, const std::string& text, const std::string& buttonText, std::function<void(int, IPlugin1*)> fn, IPlugin1* owner = nullptr);

		inline bool Has() { return m_notifs.size() > 0; }
		void Render();

	private:
		struct Entry {
			Entry()
			{
				ID = 0;
				Text = ButtonText = "";
				Timer.Restart();
				Owner = nullptr;
				Handler = nullptr;
			}
			Entry(int id, const std::string& text, const std::string& btn, std::function<void(int, IPlugin1*)> fn, IPlugin1* owner = nullptr)
			{
				ID = id;
				Text = text;
				ButtonText = btn;
				Timer.Restart();
				Owner = owner;
				Handler = fn;
			}

			int ID;
			std::string Text;
			std::string ButtonText;
			eng::Timer Timer;
			IPlugin1* Owner;

			std::function<void(int, IPlugin1*)> Handler;
		};
		std::vector<Entry> m_notifs;
	};
}