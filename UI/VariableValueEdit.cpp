#include "VariableValueEdit.h"
#include "../ImGUI/imgui.h"

namespace ed
{
	VariableValueEditUI::VariableValueEditUI()
	{
		Close();
	}
	void VariableValueEditUI::Update()
	{
		if (m_var != nullptr) {
			ImGui::Text("Variable \"%s\"", m_var->Name);

			ImGui::PushItemWidth(-1);

			switch (m_var->GetType()) {
				case ed::ShaderVariable::ValueType::Float4x4:
				case ed::ShaderVariable::ValueType::Float3x3:
				case ed::ShaderVariable::ValueType::Float2x2:
					for (int y = 0; y < m_var->GetColumnCount(); y++)
						ImGui::InputFloat4(("##valuedit" + std::to_string(y)).c_str(), m_var->AsFloatPtr(0, y));
				break;
				case ed::ShaderVariable::ValueType::Float1:
					ImGui::InputFloat("##valuedit", m_var->AsFloatPtr());
					break;
				case ed::ShaderVariable::ValueType::Float2:
					ImGui::InputFloat2("##valuedit", m_var->AsFloatPtr());
					break;
				case ed::ShaderVariable::ValueType::Float3:
					ImGui::InputFloat3("##valuedit", m_var->AsFloatPtr());
					break;
				case ed::ShaderVariable::ValueType::Float4:
					ImGui::InputFloat4("##valuedit_", m_var->AsFloatPtr());
					break;
				case ed::ShaderVariable::ValueType::Integer1:
					ImGui::InputInt("##valuedit", m_var->AsIntegerPtr());
					break;
				case ed::ShaderVariable::ValueType::Integer2:
					ImGui::InputInt2("##valuedit", m_var->AsIntegerPtr());
					break;
				case ed::ShaderVariable::ValueType::Integer3:
					ImGui::InputInt3("##valuedit", m_var->AsIntegerPtr());
					break;
				case ed::ShaderVariable::ValueType::Integer4:
					ImGui::InputInt4("##valuedit", m_var->AsIntegerPtr());
					break;
				case ed::ShaderVariable::ValueType::Boolean1:
					ImGui::Checkbox("##valuedit", m_var->AsBooleanPtr());
					break;
				case ed::ShaderVariable::ValueType::Boolean2:
					ImGui::Checkbox("##valuedit1", m_var->AsBooleanPtr(0)); ImGui::SameLine();
					ImGui::Checkbox("##valuedit2", m_var->AsBooleanPtr(1));
					break;
				case ed::ShaderVariable::ValueType::Boolean3:
					ImGui::Checkbox("##valuedit1", m_var->AsBooleanPtr(0)); ImGui::SameLine();
					ImGui::Checkbox("##valuedit2", m_var->AsBooleanPtr(1)); ImGui::SameLine();
					ImGui::Checkbox("##valuedit3", m_var->AsBooleanPtr(2));
					break;
				case ed::ShaderVariable::ValueType::Boolean4:
					ImGui::Checkbox("##valuedit1", m_var->AsBooleanPtr(0)); ImGui::SameLine();
					ImGui::Checkbox("##valuedit2", m_var->AsBooleanPtr(1)); ImGui::SameLine();
					ImGui::Checkbox("##valuedit3", m_var->AsBooleanPtr(2)); ImGui::SameLine();
					ImGui::Checkbox("##valuedit4", m_var->AsBooleanPtr(3));
					break;
			}
			ImGui::PopItemWidth();
		}
	}
}
