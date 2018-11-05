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
			Message(Type type, const std::string& group, const std::string& txt)
			{
				Type = type;
				Group = group;
				Text = txt;
			}
			Type Type;
			std::string Group;
			std::string Text;
		};

		// group -> pipeline item name
		void Add(Type type, const std::string& group, const std::string& message);
		void ClearGroup(const std::string& group);

		bool CanRenderPreview();

		inline std::vector<Message>& GetMessages() { return m_msgs; }

	private:
		std::vector<Message> m_msgs;
	};
}