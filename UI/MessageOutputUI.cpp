#include "MessageOutputUI.h"
#include <imgui/imgui.h>

namespace ed
{
	void MessageOutputUI::OnEvent(const ml::Event & e)
	{}
	void MessageOutputUI::Update(float delta)
	{
		std::vector<MessageStack::Message> msgs = m_data->Messages.GetMessages();

		for (int i = 0; i < msgs.size(); i++) {
			ImVec4 color = IMGUI_MESSAGE_COLOR;
			if (msgs[i].Type == MessageStack::Type::Error)
				color = IMGUI_ERROR_COLOR;
			else if (msgs[i].Type == MessageStack::Type::Warning)
				color = IMGUI_WARNING_COLOR;

			ImGui::Text(msgs[i].Group.c_str());

			ImGui::SameLine(150);
			ImGui::TextColored(color, msgs[i].Text.c_str());
		}
	}
}