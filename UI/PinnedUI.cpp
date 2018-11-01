#include "PinnedUI.h"
#include "../ImGUI/imgui.h"
#include "CodeEditorUI.h"

namespace ed
{
	void PinnedUI::OnEvent(const ml::Event & e)
	{
	}
	void PinnedUI::Update(float delta)
	{
		for (auto var : m_pinnedVars) {
			m_editor.Open(var);
			m_editor.Update();

			ImGui::SetCursorPosX(ImGui::GetWindowWidth()/2 - 22);
			if (ImGui::Button(("CLOSE##pin_cls_" + std::string(var->Name)).c_str(), ImVec2(44, 0)))
				Remove(var->Name);

			ImGui::NewLine();
			ImGui::Separator();
			ImGui::NewLine();
		}

		if (m_pinnedVars.size() == 0)
			ImGui::TextWrapped("Pin variables here for easy editing");
	}
	void PinnedUI::Add(ed::ShaderVariable* inp)
	{
		if (Contains(inp->Name)) return;

		m_pinnedVars.push_back(inp);
	}
	void PinnedUI::Remove(const char * name)
	{
		for (int i = 0; i < m_pinnedVars.size(); i++) {
			if (strcmp(name, m_pinnedVars[i]->Name) == 0) {
				m_pinnedVars.erase(m_pinnedVars.begin() + i);
				return;
			}
		}
	}
	bool PinnedUI::Contains(const char* data)
	{
		for (auto var : m_pinnedVars)
			if (strcmp(var->Name, data) == 0)
				return true;
		return false;
	}
}