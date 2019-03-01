#include "PreviewUI.h"
#include "PropertyUI.h"
#include "../Objects/DefaultState.h"
#include "../Objects/SystemVariableManager.h"
#include "../ImGUI/imgui_internal.h"

#define STATUSBAR_HEIGHT 25
#define STATUSBAR_ACTIVE true

namespace ed
{
	void PreviewUI::OnEvent(const ml::Event & e)
	{
		if (e.Type == ml::EventType::MouseButtonPress)
			m_mouseContact = ImVec2(e.MouseButton.Position.x, e.MouseButton.Position.y);
	}
	void PreviewUI::Update(float delta)
	{
		if (!m_data->Messages.CanRenderPreview()) {
			ImGui::TextColored(IMGUI_ERROR_COLOR, "Can not display preview - there are some errors you should fix.");
			return;
		}

		ImVec2 imageSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y - STATUSBAR_HEIGHT * STATUSBAR_ACTIVE));

		ed::RenderEngine* renderer = &m_data->Renderer;
		renderer->Render(imageSize.x, imageSize.y);

		// display the image on the imgui window
		ID3D11ShaderResourceView* view = renderer->GetTexture().GetView();
		ImGui::Image(view, imageSize);

		// render the gizmo if necessary
		if (m_pick != nullptr) {
			// recreate render texture if size is changed
			if (m_lastSize.x != imageSize.x || m_lastSize.y != imageSize.y) {
				m_lastSize = DirectX::XMINT2(imageSize.x, imageSize.y);
				m_rt.Create(*m_data->GetOwner(), m_lastSize, ml::Resource::ShaderResource, true);
				m_rtView.Create(*m_data->GetOwner(), m_rt);
			}
			m_rt.Bind();
			m_rt.Clear();
			m_rt.ClearDepthStencil(1.0f, 0);

			m_gizmo.SetProjectionMatrix(SystemVariableManager::Instance().GetProjectionMatrix());
			m_gizmo.SetViewMatrix(SystemVariableManager::Instance().GetCamera().GetMatrix());
			m_gizmo.Render();

			m_data->GetOwner()->Bind();

			ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMin().y);
			ImGui::Image(m_rtView.GetView(), imageSize);
		}

		// update system variable mouse position value
		SystemVariableManager::Instance().SetMousePosition((ImGui::GetMousePos().x - ImGui::GetCursorScreenPos().x - ImGui::GetScrollX()) / imageSize.x,
			(imageSize.y + (ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y - ImGui::GetScrollY())) / imageSize.y);

		// mouse controls for preview window
		if (ImGui::IsItemHovered()) {
			// zoom in/out if needed
			SystemVariableManager::Instance().GetCamera().Move(-ImGui::GetIO().MouseWheel);

			// handle left click - selection
			if (ImGui::IsMouseClicked(0)) {
				// screen space position
				DirectX::XMFLOAT2 s = SystemVariableManager::Instance().GetMousePosition();
				s.x *= imageSize.x;
				s.y *= imageSize.y;

				if ((m_pick != nullptr && m_gizmo.Click(s.x, s.y, m_lastSize.x, m_lastSize.y) == -1) || m_pick == nullptr) {
					renderer->Pick(s.x, s.y, [&](PipelineItem* item) {
						((PropertyUI*)m_ui->Get("Properties"))->Open(item);
						m_pick = item;

						if (item != nullptr) {
							if (item->Type == PipelineItem::ItemType::Geometry) {
								pipe::GeometryItem* geo = (pipe::GeometryItem*)item->Data;
								m_gizmo.SetTransform(&geo->Position, &geo->Scale, &geo->Rotation);
							} else if (item->Type == PipelineItem::ItemType::OBJModel) {
								pipe::OBJModel* obj = (pipe::OBJModel*)item->Data;
								m_gizmo.SetTransform(&obj->Position, &obj->Scale, &obj->Rotation);
							}
						}
					});
				}
			}

			// handle right mouse dragging - camera
			if (ImGui::IsMouseDown(1)) {
				POINT point;
				GetCursorPos(&point);
				ScreenToClient(m_data->GetOwner()->GetWindowHandle(), &point);


				// get the delta from the last position
				int dX = point.x - m_mouseContact.x;
				int dY = point.y - m_mouseContact.y;

				// save the last position
				m_mouseContact = ImVec2(point.x, point.y);

				// rotate the camera according to the delta
				SystemVariableManager::Instance().GetCamera().RotateX(dX);
				SystemVariableManager::Instance().GetCamera().RotateY(dY);
			}
			// handle left mouse dragging - moving objects if selected
			else if (ImGui::IsMouseDown(0)) {
				// screen space position
				DirectX::XMFLOAT2 s = SystemVariableManager::Instance().GetMousePosition();
				s.x *= imageSize.x;
				s.y *= imageSize.y;

				m_gizmo.Move(s.x, s.y);
			}
		}

		// status bar
		if (STATUSBAR_ACTIVE) {
			ImGui::Separator();
			ImGui::Text("FPS: %.2f", 1 / delta);
			ImGui::SameLine();
			ImGui::Text("|");
			ImGui::SameLine();

			if (m_pickMode == 0) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button("P##pickModePos", ImVec2(17, 17)) && m_pickMode != 0) m_pickMode = 0;
			else if (m_pickMode == 0) ImGui::PopStyleColor();
			ImGui::SameLine();

			if (m_pickMode == 1) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button("S##pickModeScl", ImVec2(17, 17)) && m_pickMode != 1) m_pickMode = 1;
			else if (m_pickMode == 1) ImGui::PopStyleColor();
			ImGui::SameLine();

			if (m_pickMode == 2) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button("R##pickModeRot", ImVec2(17, 17)) && m_pickMode != 2) m_pickMode = 2;
			else if (m_pickMode == 2) ImGui::PopStyleColor();
			ImGui::SameLine();

			if (m_pick != nullptr) {
				ImGui::Text("|");
				ImGui::SameLine();
				ImGui::Text("Pick: %s", m_pick->Name);
			}
		}
	}
}