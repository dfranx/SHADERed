#include "ObjectListUI.h"
#include "../ImGUI/imgui_internal.h"

#define IMAGE_CONTEXT_WIDTH 150

namespace ed
{
	void ObjectListUI::OnEvent(const ml::Event & e)
	{
	}
	void ObjectListUI::Update(float delta)
	{
		ImVec2 containerSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y));
		bool itemMenuOpened = false;
		auto items = m_data->Objects.GetObjects();
		auto passes = m_data->Pipeline.GetList();

		ImGui::BeginChild("test", containerSize);

		for (int i = 0; i < items.size(); i++) {
			ml::ShaderResourceView* srv = m_data->Objects.GetSRV(items[i]);
			ml::Image* img = m_data->Objects.GetImage(items[i]);
			DirectX::Image imgData = img->GetImage()->GetImages()[0];

			ImGui::Selectable(items[i].c_str());

			if (ImGui::BeginPopupContextItem(std::string("##context" + items[i]).c_str())) {
				itemMenuOpened = true;
				ImGui::Text("Preview");
				ImGui::Image(srv->GetView(), ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgData.height/imgData.width)* IMAGE_CONTEXT_WIDTH));

				ImGui::Separator();


				if (ImGui::BeginMenu("Bind")) {
					for (int j = 0; j < passes.size(); j++) {
						bool isBound = m_data->Objects.IsBound(items[i], passes[j]);
						if (ImGui::MenuItem(passes[j]->Name, 0, isBound)) {
							if (!isBound)
								m_data->Objects.Bind(items[i], passes[j]);
							else
								m_data->Objects.Unbind(items[i], passes[j]);
						}
					}
					ImGui::EndMenu();
				}

				if (ImGui::Selectable("Delete"))
					m_data->Objects.Remove(items[i]);

				ImGui::EndPopup();
			}
		}

		ImGui::EndChild();
		if (!itemMenuOpened && ImGui::BeginPopupContextItem("##context_main_objects")) {
			if (ImGui::Selectable("Properties")) {

			}

			if (ImGui::Selectable("Delete")) {
			}

			ImGui::EndPopup();
		}
	}
}