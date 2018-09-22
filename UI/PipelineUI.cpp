#include "PipelineUI.h"
#include "PropertyUI.h"
#include "../GUIManager.h"
#include "../ImGUI/imgui.h"

namespace ed
{
	void PipelineUI::OnEvent(const ml::Event & e)
	{}
	void PipelineUI::Update(float delta)
	{
		std::vector<ed::PipelineManager::Item>& items = m_data->Pipeline.GetList();

		for (int i = 0; i < items.size(); i++) {
			m_renderUpDown(items, i);
			if (items[i].Type == ed::PipelineItem::ShaderFile)
				m_addShader(items[i].Name, (ed::pipe::ShaderItem*)items[i].Data);
			else if (items[i].Type == ed::PipelineItem::Geometry)
				m_addGeometry(items[i].Name);
			m_renderContext(items, i);
		}
	}
	void PipelineUI::m_renderUpDown(std::vector<ed::PipelineManager::Item>& items, int index)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::Button(std::string("U##" + items[index].Name).c_str())) {
			if (index != 0) {
				ed::PipelineManager::Item temp = items[index - 1];
				items[index - 1] = items[index];
				items[index] = temp;
			}
		}
		ImGui::SameLine(0, 0);

		if (ImGui::Button(std::string("D##" + items[index].Name).c_str())) {
			if (index != items.size() - 1) {
				ed::PipelineManager::Item temp = items[index + 1];
				items[index + 1] = items[index];
				items[index] = temp;
			}
		}
		ImGui::SameLine(0, 4);

		ImGui::PopStyleColor();
	}
	void PipelineUI::m_renderContext(std::vector<ed::PipelineManager::Item>& items, int index)
	{
		if (ImGui::BeginPopupContextItem(items[index].Name.c_str())) {
			if (ImGui::Selectable("Delete")) {
				
				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")));
				if (props->CurrentItemName() == items[index].Name)
					props->Open(nullptr);

				m_data->Pipeline.Remove(items[index].Name);
			}

			if (ImGui::Selectable("Properties"))
				(reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")))->Open(&items[index]);

			ImGui::EndPopup();
		}
	}
	void PipelineUI::m_addShader(const std::string& name, ed::pipe::ShaderItem* data)
	{
		std::string type = "PS";
		if (data->Type == ed::pipe::ShaderItem::VertexShader)
			type = "VS";
		else if (data->Type == ed::pipe::ShaderItem::GeometryShader)
			type = "GS";

		ImGui::Text(type.c_str());
		ImGui::SameLine();

		ImGui::Indent(60);
		ImGui::Selectable(name.c_str());
		ImGui::Unindent(60);
	}
	void PipelineUI::m_addGeometry(const std::string & name)
	{
		ImGui::Indent(60);
		ImGui::Selectable(name.c_str());
		ImGui::Unindent(60);
	}
}