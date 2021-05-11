#include <SHADERed/UI/Debug/ValuesUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <imgui/imgui.h>

namespace ed {
	void DebugValuesUI::Refresh()
	{
		m_cachedGlobals.clear();
		m_cachedLocals.clear();

		std::stringstream ss;
		spvm_state_t vm = m_data->Debugger.GetVM();

		for (spvm_word i = 0; i < vm->owner->bound; i++) {
			spvm_result_t slot = &vm->results[i];

			if ((slot->type == spvm_result_type_variable || slot->type == spvm_result_type_function_parameter) && slot->name != nullptr) {
				if (slot->owner == nullptr) {
					spvm_result_t vtype = spvm_state_get_type_info(vm->results, &vm->results[slot->pointer]);
					m_data->Debugger.GetVariableValueAsString(ss, m_data->Debugger.GetVM(), vtype, slot->members, slot->member_count, "");
					m_cachedGlobals[slot->name] = ss.str();
					ss.str(std::string());
				} else if (slot->owner == vm->current_function) {
					spvm_result_t vtype = spvm_state_get_type_info(vm->results, &vm->results[slot->pointer]);
					m_data->Debugger.GetVariableValueAsString(ss, m_data->Debugger.GetVM(), vtype, slot->members, slot->member_count, "");
					m_cachedLocals[slot->name] = ss.str();
					ss.str(std::string());
				}
			}
		}
	}
	void DebugValuesUI::OnEvent(const SDL_Event& e)
	{
	}
	void DebugValuesUI::Update(float delta)
	{
		// Main window
		ImGui::BeginChild("##values_viewarea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		ImGui::Text("Globals:");
		ImGui::Separator();
		ImGui::Columns(2);
		for (const auto& key : m_cachedGlobals) {
			ImGui::Text("%s", key.first.c_str());
			ImGui::NextColumn();
			ImGui::Text("%s", key.second.c_str());
			ImGui::NextColumn();
			ImGui::Separator();
		}
		ImGui::Columns();

		ImGui::NewLine();

		ImGui::Text("Locals:");
		ImGui::Separator();
		ImGui::Columns(2);
		for (const auto& key : m_cachedLocals) {
			ImGui::Text("%s", key.first.c_str());
			ImGui::NextColumn();
			ImGui::Text("%s", key.second.c_str());
			ImGui::NextColumn();
			ImGui::Separator();
		}
		ImGui::Columns();

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
	}
}