#include "PreviewUI.h"
#include "../ImGUI/imgui.h"

namespace ed
{
	void PreviewUI::OnEvent(const ml::Event & e)
	{}
	void PreviewUI::Update(float delta)
	{
		ImVec2 padding = ImGui::GetStyle().WindowPadding;
		ImVec2 imageSize = ImVec2(ImGui::GetWindowWidth() - padding.x, ImGui::GetWindowHeight() - 2 * padding.y);

		ed::RenderEngine* renderer = &m_data->Renderer;
		renderer->Render(imageSize.x, imageSize.y);

		ID3D11ShaderResourceView* view = renderer->GetTexture().GetView();
		ImGui::Image(view, imageSize);
		if (ImGui::IsItemClicked()) {
			/*
				TODO: camera movement
			*/
		}
	}
}