#include "PropertyUI.h"
#include "../ImGUI/imgui.h"

namespace ed
{
	void PropertyUI::OnEvent(const ml::Event & e)
	{}
	void PropertyUI::Update(float delta)
	{
		if (m_current != nullptr) {
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

			ImGui::Text(m_current->Name);
			ImGui::PopStyleColor();

			ImGui::Columns(2, "##content_columns");
			ImGui::SetColumnWidth(0, ImGui::GetWindowSize().x * 0.3f);

			ImGui::Separator();
			
			ImGui::Text("Name:");
			ImGui::NextColumn();

			ImGui::PushItemWidth(-40);
			ImGui::InputText("##pui_itemname", m_itemName, PIPELINE_ITEM_NAME_LENGTH);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("Ok##pui_itemname", ImVec2(-1, 0))) {
				if (m_data->Pipeline.Has(m_itemName))
					ImGui::OpenPopup("ERROR##pui_itemname_taken");
				else if (strlen(m_itemName) <= 2)
					ImGui::OpenPopup("ERROR##pui_itemname_short");
				else memcpy(m_current->Name, m_itemName, PIPELINE_ITEM_NAME_LENGTH);
			}
			ImGui::NextColumn();

			ImGui::Separator();

			ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f));
			if (ImGui::BeginPopupModal("ERROR##pui_itemname_taken", 0, ImGuiWindowFlags_NoResize)) {
				ImGui::Text("There is already an item with name \"%s\"", m_itemName);
				if (ImGui::Button("Ok")) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
			if (ImGui::BeginPopupModal("ERROR##pui_itemname_short", 0, ImGuiWindowFlags_NoResize)) {
				ImGui::Text("The name must be at least 3 characters long.");
				if (ImGui::Button("Ok")) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
			ImGui::PopStyleVar();

			if (m_current->Type == ed::PipelineItem::ShaderFile) {
				ed::pipe::ShaderItem* item = reinterpret_cast<ed::pipe::ShaderItem*>(m_current->Data);

				/* shader path */
				ImGui::Text("Path:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-40);
				ImGui::InputText("##pui_shaderpath", item->FilePath, 512);
				ImGui::PopItemWidth();
				ImGui::SameLine();
				ImGui::Button("...", ImVec2(-1, 0));
				ImGui::NextColumn();

				ImGui::Separator();

				/* shader entry */
				ImGui::Text("Entry:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				ImGui::InputText("##pui_shaderentry", item->Entry, 32);
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				/* shader type */
				static const char* items[] =
				{
					"Pixel\0",
					"Vertex\0",
					"Geometry\0"
				};
				ImGui::Text("Type:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				ImGui::Combo("##pui_shadertype", reinterpret_cast<int*>(&item->Type), items, IM_ARRAYSIZE(items));
				ImGui::PopItemWidth();
			}
			else if (m_current->Type == ed::PipelineItem::Geometry) {
				ed::pipe::GeometryItem* item = reinterpret_cast<ed::pipe::GeometryItem*>(m_current->Data);

				/* position */
				ImGui::Text("Position:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				m_createInputFloat3("##pui_geopos", item->Position);
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* shader type */
				static const char* items[] =
				{
					"Undefined",
					"PointList",
					"LineList",
					"LineStrip",
					"TriangleList",
					"TriangleStrip",
					"LineListAdjecent",
					"LineStripAdjecent",
					"TriangleListAdjecent",
					"TriangleStripAdjecent"
				};
				ImGui::Text("Type:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				ImGui::Combo("##pui_shadertype", reinterpret_cast<int*>(&item->Topology), items, IM_ARRAYSIZE(items));
				ImGui::PopItemWidth();
			}

			ImGui::NextColumn();
			ImGui::Separator();
			ImGui::Columns(1);
		} else {
			ImGui::Text("Right click on an item -> Properties");
		}
	}
	void PropertyUI::Open(ed::PipelineManager::Item * item)
	{
		if (item != nullptr) memcpy(m_itemName, item->Name, PIPELINE_ITEM_NAME_LENGTH);

		m_current = item;
	}
	void PropertyUI::m_createInputFloat3(const char* label, DirectX::XMFLOAT3& flt)
	{
		float val[3] = { flt.x , flt.y, flt.z };
		ImGui::InputFloat3(label, val, 5);
		flt = DirectX::XMFLOAT3(val);
	}
}