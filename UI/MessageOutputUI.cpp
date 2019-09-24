#include "MessageOutputUI.h"
#include "../Objects/Settings.h"
#include "../Options.h"
#include <imgui/imgui.h>

namespace ed
{
	void MessageOutputUI::OnEvent(const SDL_Event& e)
	{}
	void MessageOutputUI::Update(float delta)
	{
		const std::vector<MessageStack::Message>& msgs = m_data->Messages.GetMessages();

		ImGui::Columns(4);

		ImGui::SetColumnWidth(0, 150.0f);
		ImGui::SetColumnWidth(1, 60.0f);
		ImGui::SetColumnWidth(2, 60.0f);

		ImGui::Text("Shader Pass"); ImGui::NextColumn();
		ImGui::Text("Source"); ImGui::NextColumn();
		ImGui::Text("Line"); ImGui::NextColumn();
		ImGui::Text("Message"); ImGui::NextColumn();
		ImGui::Separator();

		for (int i = 0; i < msgs.size(); i++) {
			const MessageStack::Message* m = &msgs[i];

			ImVec4 color = IMGUI_MESSAGE_COLOR;
			if (m->MType == MessageStack::Type::Error)
				color = IMGUI_ERROR_COLOR;
			else if (m->MType == MessageStack::Type::Warning)
				color = IMGUI_WARNING_COLOR;

			ImGui::Text(m->Group.c_str());
			ImGui::NextColumn();

			if (m->Shader != -1)
				ImGui::Text(m->Shader == 0 ? "VS" : (m->Shader == 1 ? "PS" : (m->Shader == 2 ? "GS" : "CS")));
			ImGui::NextColumn();

			if (m->Line != -1)
				ImGui::Text(std::to_string(m->Line).c_str());
			ImGui::NextColumn();

			ImGui::TextColored(color, m->Text.c_str());
			ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}
}