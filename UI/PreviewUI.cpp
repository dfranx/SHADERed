#include "PreviewUI.h"
#include "../Objects/SystemVariableManager.h"

namespace ed
{
	void PreviewUI::OnEvent(const ml::Event & e)
	{
		if (e.Type == ml::EventType::MouseButtonPress)
			m_mouseContact = ImVec2(e.MouseButton.Position.x, e.MouseButton.Position.y);
	}
	void PreviewUI::Update(float delta)
	{
		ImVec2 padding = ImGui::GetStyle().WindowPadding;
		ImVec2 imageSize = ImVec2(ImGui::GetWindowWidth() - padding.x, ImGui::GetWindowHeight() - 2 * padding.y);

		ed::RenderEngine* renderer = &m_data->Renderer;
		renderer->Render(imageSize.x, imageSize.y);

		// update system variable mouse position value
		SystemVariableManager::Instance().SetMousePosition(ImGui::GetCursorScreenPos().x - ImGui::GetMousePos().x, ImGui::GetCursorScreenPos().y - ImGui::GetMousePos().y);
		
		ID3D11ShaderResourceView* view = renderer->GetTexture().GetView();
		ImGui::Image(view, imageSize);

		if (ImGui::IsItemHovered()) {
			// zoom in/out if needed
			SystemVariableManager::Instance().GetCamera().Move(-ImGui::GetIO().MouseWheel);

			// handle right mouse dragging - camera
			if (ImGui::IsMouseDown(1)) {
				// get the delta from the last position
				int dX = ImGui::GetIO().MousePos.x - m_mouseContact.x;
				int dY = ImGui::GetIO().MousePos.y - m_mouseContact.y;

				// save the last position
				m_mouseContact = ImGui::GetIO().MousePos;

				// rotate the camera according to the delta
				SystemVariableManager::Instance().GetCamera().RotateX(dX);
				SystemVariableManager::Instance().GetCamera().RotateY(dY);
			}
		}
	}
}