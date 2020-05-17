#include <SHADERed/Objects/MessageStack.h>

namespace ed {
	MessageStack::MessageStack()
	{
		BuildOccured = false;
	}
	MessageStack::~MessageStack()
	{
	}
	void MessageStack::Add(const std::vector<Message>& msgs)
	{
		m_msgs.insert(m_msgs.end(), msgs.begin(), msgs.end());
	}
	void MessageStack::Add(Type type, const std::string& group, const std::string& message, int ln, ShaderStage sh)
	{
		m_msgs.push_back({ type, group, message, ln, sh });
	}
	void MessageStack::ClearGroup(const std::string& group, int type)
	{
		for (int i = 0; i < m_msgs.size(); i++)
			if (m_msgs[i].Group == group && (type == -1 || m_msgs[i].MType == (ed::MessageStack::Type)type)) {
				m_msgs.erase(m_msgs.begin() + i);
				i--;
			}
	}
	int MessageStack::GetGroupWarningMsgCount(const std::string& group)
	{
		int cnt = 0;
		for (int i = 0; i < m_msgs.size(); i++)
			if (m_msgs[i].Group == group && m_msgs[i].MType == ed::MessageStack::Type::Warning)
				cnt++;
		return cnt;
	}
	int MessageStack::GetErrorAndWarningMsgCount()
	{
		int cnt = 0;
		for (int i = 0; i < m_msgs.size(); i++)
			if (m_msgs[i].MType == ed::MessageStack::Type::Warning || m_msgs[i].MType == ed::MessageStack::Type::Error)
				cnt++;
		return cnt;
	}
	int MessageStack::GetGroupErrorAndWarningMsgCount(const std::string& group)
	{
		int cnt = 0;
		for (int i = 0; i < m_msgs.size(); i++)
			if ((m_msgs[i].MType == ed::MessageStack::Type::Warning || m_msgs[i].MType == ed::MessageStack::Type::Error) && m_msgs[i].Group == group)
				cnt++;
		return cnt;
	}
	void MessageStack::RenameGroup(const std::string& group, const std::string& newName)
	{
		for (int i = 0; i < m_msgs.size(); i++)
			if (m_msgs[i].Group == group)
				m_msgs[i].Group = newName;
	}
	bool MessageStack::CanRenderPreview()
	{
		for (int i = 0; i < m_msgs.size(); i++)
			if (m_msgs[i].MType == Type::Error)
				return false;
		return true;
	}
}