#include "PropertyUI.h"
#include "../ImGUI/imgui.h"

namespace ed
{
	void PropertyUI::OnEvent(const ml::Event & e)
	{}
	void PropertyUI::Update(float delta)
	{
		if (m_current != nullptr) {
			ImGui::Text(m_current->Name.c_str());
			ImGui::Separator();
			ImGui::Separator();

			if (m_current->Type == ed::PipelineItem::ShaderFile) {
				ed::pipe::ShaderItem* item = reinterpret_cast<ed::pipe::ShaderItem*>(m_current->Data);

				/* shader path */
				ImGui::Text("Path:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-40);
				ImGui::InputText("##pui_shaderpath", item->FilePath, 512);
				ImGui::PopItemWidth();
				ImGui::SameLine();
				ImGui::Button("...", ImVec2(-1, 0));
				
				/* shader type */
				static const char* items[] =
				{
					"Pixel\0",
					"Vertex\0",
					"Geometry\0"
				};
				ImGui::Text("Type:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::ListBox("##pui_shadertype", reinterpret_cast<int*>(&item->Type), items, 3);
				ImGui::PopItemWidth();
			} else if (m_current->Type == ed::PipelineItem::Geometry) {
				ed::pipe::GeometryItem* item = reinterpret_cast<ed::pipe::GeometryItem*>(m_current->Data);

				/* position */
				ImGui::Text("Position:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				m_createInputFloat3("##pui_geopos", item->Position);
				ImGui::PopItemWidth();
			}
		} else {
			ImGui::Text("Right click on an item -> Properties");
		}
	}
	void PropertyUI::Open(ed::PipelineManager::Item * item)
	{
		m_current = item;
	}
	void PropertyUI::m_createInputFloat3(const char* label, DirectX::XMFLOAT3& flt)
	{
		float val[3] = { flt.x , flt.y, flt.z };
		ImGui::InputFloat3(label, val, 5);
		flt = DirectX::XMFLOAT3(val);
	}
}