#include "MessageStack.h"

namespace ed
{
	MessageStack::MessageStack()
	{
		BuildOccured = false;
	}
	MessageStack::~MessageStack()
	{}
	void MessageStack::Add(Type type, const std::string & group, const std::string & message, int ln, int sh)
	{
		m_msgs.push_back({ type, group, message, ln, sh });
	}
	void MessageStack::ClearGroup(const std::string & group)
	{
		for (int i = 0; i < m_msgs.size(); i++)
			if (m_msgs[i].Group == group) {
				m_msgs.erase(m_msgs.begin() + i);
				i--;
			}
	}
	bool MessageStack::CanRenderPreview()
	{
		for (int i = 0; i < m_msgs.size(); i++)
			if (m_msgs[i].Type == Type::Error)
				return false;
		return true;
	}
}