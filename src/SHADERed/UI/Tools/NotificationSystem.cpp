#include <SHADERed/UI/Tools/NotificationSystem.h>
#include <SHADERed/Objects/Settings.h>

#include <glm/glm.hpp>
#include <imgui/imgui.h>

namespace ed {
	void NotificationSystem::Add(int id, const std::string& text, const std::string& buttonText, std::function<void(int, IPlugin1*)> fn, IPlugin1* owner)
	{
		m_notifs.push_back(Entry(id, text, buttonText, fn, owner));
	}
	void NotificationSystem::Render()
	{
		const float DISTANCE = 15.0f;

		ImGuiIO& io = ImGui::GetIO();
		ImVec2 window_pos_pivot = ImVec2(1.0f, 1.0f);
		ImVec4 textClr = ImGui::GetStyleColorVec4(ImGuiCol_Text);
		ImVec4 windowClr = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);

		float yOffset = 0.0f;

		ImGui::PushStyleColor(ImGuiCol_WindowBg, textClr);
		ImGui::PushStyleColor(ImGuiCol_Text, windowClr);
		for (int i = 0; i < m_notifs.size(); i++) {
			bool stayOpen = true;
			ImVec2 window_pos = ImVec2(io.DisplaySize.x - DISTANCE, io.DisplaySize.y - DISTANCE - yOffset);

			ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
			ImGui::SetNextWindowBgAlpha(1.0f - glm::clamp(m_notifs[i].Timer.GetElapsedTime() - 5.0f, 0.0f, 2.0f) / 2.0f); // Transparent background
			ImGui::SetNextWindowContentSize(ImVec2(Settings::Instance().CalculateSize(350), 0));
			
			if (ImGui::Begin(("##updateNotification" + std::to_string(i)).c_str(), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
				if (ImGui::IsWindowHovered())
					m_notifs[i].Timer.Restart();

				ImGui::TextWrapped("%s", m_notifs[i].Text.c_str());

				if (m_notifs[i].Handler && m_notifs[i].ButtonText.size() > 0) {
					if (ImGui::Button(m_notifs[i].ButtonText.c_str())) {
						m_notifs[i].Handler(m_notifs[i].ID, m_notifs[i].Owner);
						stayOpen = false;
					}
				}
			}

			if (m_notifs[i].Timer.GetElapsedTime() > 7.0f)
				stayOpen = false;

			if (!stayOpen) {
				m_notifs.erase(m_notifs.begin() + i);
				i--;
			} else
				yOffset += ImGui::GetWindowContentRegionMax().y + DISTANCE;

			ImGui::End();
		}

		ImGui::PopStyleColor(2);
	}
}