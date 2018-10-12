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
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth()-(51+ImGui::GetStyle().WindowPadding.x*2));
			ImGui::Button("V", ImVec2(25, 0));
			ImGui::SameLine();
			ImGui::Button("F", ImVec2(25, 0)); // TODO: functions

			ImGui::PushItemWidth(-1);

			switch (m_var->GetType()) {
				case ed::ShaderVariable::ValueType::Float4x4:
				case ed::ShaderVariable::ValueType::Float3x3:
				case ed::ShaderVariable::ValueType::Float2x2:
					for (int y = 0; y < m_var->GetColumnCount(); y++)
						ImGui::DragFloat4(("##valuedit" + std::string(m_var->Name) + std::to_string(y)).c_str(), m_var->AsFloatPtr(0, y), 0.01f);
				break;
				case ed::ShaderVariable::ValueType::Float1:
					ImGui::DragFloat(("##valuedit" + std::string(m_var->Name)).c_str(), m_var->AsFloatPtr(), 0.01f);
					break;
				case ed::ShaderVariable::ValueType::Float2:
					ImGui::DragFloat2(("##valuedit" + std::string(m_var->Name)).c_str(), m_var->AsFloatPtr(), 0.01f);
					break;
				case ed::ShaderVariable::ValueType::Float3:
					ImGui::DragFloat3(("##valuedit" + std::string(m_var->Name)).c_str(), m_var->AsFloatPtr(), 0.01f);
					break;
				case ed::ShaderVariable::ValueType::Float4:
					ImGui::DragFloat4(("##valuedit_" + std::string(m_var->Name)).c_str(), m_var->AsFloatPtr(), 0.01f);
					break;
				case ed::ShaderVariable::ValueType::Integer1:
					ImGui::DragInt(("##valuedit" + std::string(m_var->Name)).c_str(), m_var->AsIntegerPtr(), 0.3f);
					break;
				case ed::ShaderVariable::ValueType::Integer2:
					ImGui::DragInt2(("##valuedit" + std::string(m_var->Name)).c_str(), m_var->AsIntegerPtr(), 0.3f);
					break;
				case ed::ShaderVariable::ValueType::Integer3:
					ImGui::DragInt3(("##valuedit" + std::string(m_var->Name)).c_str(), m_var->AsIntegerPtr(), 0.3f);
					break;
				case ed::ShaderVariable::ValueType::Integer4:
					ImGui::DragInt4(("##valuedit" + std::string(m_var->Name)).c_str(), m_var->AsIntegerPtr(), 0.3f);
					break;
				case ed::ShaderVariable::ValueType::Boolean1:
					ImGui::Checkbox(("##valuedit" + std::string(m_var->Name)).c_str(), m_var->AsBooleanPtr());
					break;
				case ed::ShaderVariable::ValueType::Boolean2:
					ImGui::Checkbox(("##valuedit1" + std::string(m_var->Name)).c_str(), m_var->AsBooleanPtr(0)); ImGui::SameLine();
					ImGui::Checkbox(("##valuedit2" + std::string(m_var->Name)).c_str(), m_var->AsBooleanPtr(1));
					break;
				case ed::ShaderVariable::ValueType::Boolean3:
					ImGui::Checkbox(("##valuedit1" + std::string(m_var->Name)).c_str(), m_var->AsBooleanPtr(0)); ImGui::SameLine();
					ImGui::Checkbox(("##valuedit2" + std::string(m_var->Name)).c_str(), m_var->AsBooleanPtr(1)); ImGui::SameLine();
					ImGui::Checkbox(("##valuedit3" + std::string(m_var->Name)).c_str(), m_var->AsBooleanPtr(2));
					break;
				case ed::ShaderVariable::ValueType::Boolean4:
					ImGui::Checkbox(("##valuedit1" + std::string(m_var->Name)).c_str(), m_var->AsBooleanPtr(0)); ImGui::SameLine();
					ImGui::Checkbox(("##valuedit2" + std::string(m_var->Name)).c_str(), m_var->AsBooleanPtr(1)); ImGui::SameLine();
					ImGui::Checkbox(("##valuedit3" + std::string(m_var->Name)).c_str(), m_var->AsBooleanPtr(2)); ImGui::SameLine();
					ImGui::Checkbox(("##valuedit4" + std::string(m_var->Name)).c_str(), m_var->AsBooleanPtr(3));
					break;
			}
			ImGui::PopItemWidth();
		}
	}
}
