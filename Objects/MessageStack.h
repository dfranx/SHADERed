#pragma once
#include <string>
#include <vector>

namespace ed
{
	class MessageStack
	{
	public:
		MessageStack();
		~MessageStack();

		enum class Type
		{
			Error,
			Warning,
			Message
		};
		class Message
		{
		public:
			Message()
			{
				Type = Type::Error;
				Group = "";
				Text = "";
			}
			Message(Type type, const std::string& group, const std::string& txt, int line = -1, int shader = -1)
			{
				Type = type;
				Group = group;
				Text = txt;
				Line = line;
				Shader = shader;
			}
			Type Type;
			std::string Group;
			std::string Text;
			int Line;
			int Shader;
		};

		bool BuildOccured; // have we recompiled project/shader? if yes and an error has occured we can open error window
		std::string CurrentItem;	// a shader pass that we are currently compiling
		int CurrentItemType;		// 0=VS, 1=PS, 2=GS

		// group -> pipeline item name
		void Add(Type type, const std::string& group, const std::string& message, int ln = -1, int sh = -1);
		void ClearGroup(const std::string& group);

		bool CanRenderPreview();

		inline std::vector<Message>& GetMessages() { return m_msgs; }

	private:
		std::vector<Message> m_msgs;
	};
}