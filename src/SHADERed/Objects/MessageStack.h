#pragma once
#include <string>
#include <vector>
#include <SHADERed/Objects/ShaderStage.h>

namespace ed {
	class MessageStack {
	public:
		MessageStack();
		~MessageStack();

		enum class Type {
			Error,
			Warning,
			Message
		};
		class Message {
		public:
			Message()
			{
				MType = Type::Error;
				Group = "";
				Text = "";
				Line = -1;
				Shader = ShaderStage::Count;
			}
			Message(Type type, const std::string& group, const std::string& txt, int line = -1, ShaderStage shader = ShaderStage::Count)
			{
				MType = type;
				Group = group;
				Text = txt;
				Line = line;
				Shader = shader;
			}
			Type MType;
			std::string Group;
			std::string Text;
			int Line;
			ShaderStage Shader;
		};

		bool BuildOccured;		 // have we recompiled project/shader? if yes and an error has occured we can open error window
		std::string CurrentItem; // a shader pass that we are currently compiling
		
		// group -> pipeline item name
		void Add(const std::vector<Message>& msgs);
		void Add(Type type, const std::string& group, const std::string& message, int ln = -1, ShaderStage sh = ShaderStage::Count);
		void ClearGroup(const std::string& group, int type = -1);
		inline void Clear() { m_msgs.clear(); }
		int GetGroupWarningMsgCount(const std::string& group);
		int GetErrorAndWarningMsgCount();
		int GetGroupErrorAndWarningMsgCount(const std::string& group);
		void RenameGroup(const std::string& group, const std::string& newName);

		bool CanRenderPreview();

		inline std::vector<Message>& GetMessages() { return m_msgs; }

	private:
		std::vector<Message> m_msgs;
	};
}