#include "PreviewUI.h"
#include "PropertyUI.h"
#include "PipelineUI.h"
#include "../Objects/Settings.h"
#include "../Objects/DefaultState.h"
#include "../Objects/SystemVariableManager.h"
#include "../Objects/KeyboardShortcuts.h"
#include <imgui/imgui_internal.h>

#define STATUSBAR_HEIGHT 25
#define FPS_UPDATE_RATE 0.3f

namespace ed
{
	void PreviewUI::m_setupFXAA()
	{
		ml::Window* wnd = m_data->GetOwner();

		// input layout
		m_fxaaInput.Add("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0);
		m_fxaaInput.Add("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 16);

		// load shaders
		m_fxaaVS.InputSignature = &m_fxaaInput;
		m_fxaaVS.LoadFromFile(*wnd, "data/FXAA.hlsl", "FxaaVS");
		m_fxaaPS.LoadFromFile(*wnd, "data/FXAA.hlsl", "FxaaPS");

		// create vertex buffer
		m_vbQuad.Create(*wnd, 4);

		// constant buffer
		m_fxaaCB.Create(*wnd, 16);
	}
	void PreviewUI::m_fxaaCreateQuad(DirectX::XMFLOAT2 size)
	{
		// create vertex buffer
		QuadVertex2D verts[4];
		verts[0].Position = DirectX::XMFLOAT4(-size.x / 2, size.y / 2, 1.0f, 1.0f);
		verts[1].Position = DirectX::XMFLOAT4(size.x / 2, size.y / 2, 1.0f, 1.0f);
		verts[2].Position = DirectX::XMFLOAT4(-size.x / 2, -size.y / 2, 1.0f, 1.0f);
		verts[3].Position = DirectX::XMFLOAT4(size.x / 2, -size.y / 2, 1.0f, 1.0f);

		verts[0].UV = DirectX::XMFLOAT2(0.0f, 0.0f);
		verts[1].UV = DirectX::XMFLOAT2(1.0f, 0.0f);
		verts[2].UV = DirectX::XMFLOAT2(0.0f, 1.0f);
		verts[3].UV = DirectX::XMFLOAT2(1.0f, 1.0f);

		m_vbQuad.Update(verts);
	}
	void PreviewUI::m_fxaaRenderQuad()
	{
		// set SRV and CB
		m_fxaaRT.Bind();
		m_fxaaRT.Clear();
		m_fxaaRT.ClearDepthStencil(1.0f, 0);

		// render
		ml::Window* wnd = m_data->GetOwner();

		m_fxaaVS.Bind();
		m_fxaaPS.Bind();
		wnd->SetInputLayout(*m_fxaaVS.InputSignature);

		m_fxaaCB.BindVS(0);
		m_fxaaCB.BindPS(0);

		// render the quad
		D3D11_PRIMITIVE_TOPOLOGY topology;
		wnd->GetDeviceContext()->IAGetPrimitiveTopology(&topology);

		wnd->SetTopology(ml::Topology::TriangleStrip);
		m_vbQuad.Bind();
		wnd->Draw(4, 0);

		wnd->GetDeviceContext()->IASetPrimitiveTopology(topology);
	}
	void PreviewUI::m_setupShortcuts()
	{
		KeyboardShortcuts::Instance().SetCallback("Gizmo.Position", [=]() {
			m_pickMode = 0;
			m_gizmo.SetMode(m_pickMode);
		});
		KeyboardShortcuts::Instance().SetCallback("Gizmo.Scale", [=]() {
			m_pickMode = 1;
			m_gizmo.SetMode(m_pickMode);
		});
		KeyboardShortcuts::Instance().SetCallback("Gizmo.Rotation", [=]() {
			m_pickMode = 2;
			m_gizmo.SetMode(m_pickMode);
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.Delete", [=]() {
			if (m_pick != nullptr && m_hasFocus) {
				PropertyUI* props = (PropertyUI*)m_ui->Get(ViewID::Properties);
				if (props->CurrentItemName() == m_pick->Name)
					props->Open(nullptr);

				m_data->Pipeline.Remove(m_pick->Name);
				m_pick = nullptr;
			}
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.Unselect", [=]() {
			m_pick = nullptr;
		});
	}
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

		ed::Settings& settings = Settings::Instance();

		bool statusbar = settings.Preview.StatusBar;
		float fpsLimit = settings.Preview.FPSLimit;
		if (fpsLimit != m_fpsLimit) {
			m_elapsedTime = 0;
			m_fpsLimit = fpsLimit;
		}

		ImVec2 imageSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y - STATUSBAR_HEIGHT * statusbar));
		ed::RenderEngine* renderer = &m_data->Renderer;

		m_fpsUpdateTime += delta;
		m_elapsedTime += delta;
		if (m_fpsLimit <= 0 || m_elapsedTime >= 1.0f / m_fpsLimit) {
			renderer->Render(imageSize.x, imageSize.y);

			float fps = m_fpsTimer.Restart();
			if (m_fpsUpdateTime > FPS_UPDATE_RATE) {
				m_fpsDelta = fps;
				m_fpsUpdateTime -= FPS_UPDATE_RATE;
			}

			m_elapsedTime -= 1 / m_fpsLimit;
		}

		// change fxaa content size
		if (settings.Preview.FXAA && (m_fxaaLastSize.x != imageSize.x || m_fxaaLastSize.y != imageSize.y)) {
			m_cbData.wndX = 1.0f / imageSize.x;
			m_cbData.wndY = 1.0f / imageSize.y;
			m_fxaaLastSize.x = imageSize.x;
			m_fxaaLastSize.y = imageSize.y;

			m_fxaaCB.Update(&m_cbData);

			ml::Window* wnd = m_data->GetOwner();

			m_fxaaCreateQuad(m_fxaaLastSize);
			m_fxaaRT.Create(*wnd, DirectX::XMINT2(m_fxaaLastSize.x, m_fxaaLastSize.y), ml::Resource::ShaderResource, true);
			m_fxaaContent.Create(*m_data->GetOwner(), m_fxaaRT);
		}

		ID3D11ShaderResourceView* rtView = renderer->GetTexture().GetView();
		ID3D11ShaderResourceView* view = rtView;

		// apply fxaa
		if (settings.Preview.FXAA) {
			ml::Window* wnd = m_data->GetOwner();
			wnd->GetDeviceContext()->PSSetShaderResources(0, 1, &rtView);
			m_fxaaRenderQuad();
			wnd->RemoveShaderResource(0);
			view = m_fxaaContent.GetView();
		}

		// display the image on the imgui window
		ImGui::Image(view, imageSize);

		m_hasFocus = ImGui::IsWindowFocused();

		// render the gizmo if necessary
		if (m_pick != nullptr && settings.Preview.Gizmo) {
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
			m_gizmo.SetViewMatrix(SystemVariableManager::Instance().GetCamera()->GetMatrix());
			m_gizmo.Render();

			m_data->GetOwner()->Bind();

			ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMin().y);
			ImGui::Image(m_rtView.GetView(), imageSize);
		}

		// update wasd key state
		SystemVariableManager::Instance().SetKeysWASD(GetAsyncKeyState('W') != 0, GetAsyncKeyState('A') != 0, GetAsyncKeyState('S') != 0, GetAsyncKeyState('D') != 0);

		// update system variable mouse position value
		SystemVariableManager::Instance().SetMousePosition((ImGui::GetMousePos().x - ImGui::GetCursorScreenPos().x - ImGui::GetScrollX()) / imageSize.x,
			(imageSize.y + (ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y - ImGui::GetScrollY())) / imageSize.y);

		// mouse controls for preview window
		if (ImGui::IsItemHovered()) {
			bool fp = settings.Project.FPCamera;

			// zoom in/out if needed
			if (!fp)
				((ed::ArcBallCamera*)SystemVariableManager::Instance().GetCamera())->Move(-ImGui::GetIO().MouseWheel);

			// handle left click - selection
			if (((ImGui::IsMouseClicked(0) && !settings.Preview.SwitchLeftRightClick) ||
				(ImGui::IsMouseClicked(1) && settings.Preview.SwitchLeftRightClick)) &&
				settings.Preview.Gizmo)
			{
				// screen space position
				DirectX::XMFLOAT2 s = SystemVariableManager::Instance().GetMousePosition();
				s.x *= imageSize.x;
				s.y *= imageSize.y;

				if ((m_pick != nullptr && m_gizmo.Click(s.x, s.y, m_lastSize.x, m_lastSize.y) == -1) || m_pick == nullptr) {
					renderer->Pick(s.x, s.y, [&](PipelineItem* item) {
						if (settings.Preview.PropertyPick)
							((PropertyUI*)m_ui->Get(ViewID::Properties))->Open(item);
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
			if (((ImGui::IsMouseDown(0) && settings.Preview.SwitchLeftRightClick) ||
				(ImGui::IsMouseDown(1) && !settings.Preview.SwitchLeftRightClick)))
			{
				POINT point;
				GetCursorPos(&point);
				ScreenToClient(m_data->GetOwner()->GetWindowHandle(), &point);


				// get the delta from the last position
				int dX = point.x - m_mouseContact.x;
				int dY = point.y - m_mouseContact.y;

				// wrap the mouse
				{
					// screen space position
					DirectX::XMFLOAT2 s = SystemVariableManager::Instance().GetMousePosition();

					bool wrappedMouse = false;
					if (s.x < 0.01f) {
						point.x += imageSize.x - 20;
						wrappedMouse = true;
					}
					else if (s.x > 0.99f) {
						point.x -= imageSize.x - 20;
						wrappedMouse = true;
					} else if (s.y > 0.99f) {
						point.y -= imageSize.y - 20;
						wrappedMouse = true;
					} else if (s.y < 0.01f) {
						point.y += imageSize.y - 20;
						wrappedMouse = true;
					}

					if (wrappedMouse) {
						POINT spt = point;
						ClientToScreen(m_data->GetOwner()->GetWindowHandle(), &spt);

						SetCursorPos(spt.x, spt.y);
					}
				}

				// save the last position
				m_mouseContact = ImVec2(point.x, point.y);

				// rotate the camera according to the delta
				if (!fp) {
					ed::ArcBallCamera* cam = ((ed::ArcBallCamera*)SystemVariableManager::Instance().GetCamera());
					cam->RotateX(dX);
					cam->RotateY(dY);
				} else {
					ed::FirstPersonCamera* cam = ((ed::FirstPersonCamera*)SystemVariableManager::Instance().GetCamera());
					cam->Yaw(dX * 0.005f);
					cam->Pitch(-dY * 0.005f);
				}
			}
			
			// handle left mouse dragging - moving objects if selected
			else if (((ImGui::IsMouseDown(0) && !settings.Preview.SwitchLeftRightClick) ||
				(ImGui::IsMouseDown(1) && settings.Preview.SwitchLeftRightClick)) &&
				settings.Preview.Gizmo)
			{
				// screen space position
				DirectX::XMFLOAT2 s = SystemVariableManager::Instance().GetMousePosition();
				s.x *= imageSize.x;
				s.y *= imageSize.y;

				m_gizmo.Move(s.x, s.y);
			}

			// WASD key press - first person camera
			if (fp) {
				ed::FirstPersonCamera* cam = ((ed::FirstPersonCamera*)SystemVariableManager::Instance().GetCamera());
				cam->MoveUpDown((ImGui::IsKeyDown('S') - ImGui::IsKeyDown('W')) / 100.0f);
				cam->MoveLeftRight((ImGui::IsKeyDown('A') - ImGui::IsKeyDown('D')) / 100.0f);
			}
		}

		// status bar
		if (statusbar) {
			ImGui::Separator();
			ImGui::Text("FPS: %.2f", 1 / m_fpsDelta);
			ImGui::SameLine();
			ImGui::Text("|");
			ImGui::SameLine();

			if (m_pickMode == 0) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button("P##pickModePos", ImVec2(17, 17)) && m_pickMode != 0) {
				m_pickMode = 0;
				m_gizmo.SetMode(m_pickMode);
			}
			else if (m_pickMode == 0) ImGui::PopStyleColor();
			ImGui::SameLine();

			if (m_pickMode == 1) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button("S##pickModeScl", ImVec2(17, 17)) && m_pickMode != 1) {
				m_pickMode = 1;
				m_gizmo.SetMode(m_pickMode);
			}
			else if (m_pickMode == 1) ImGui::PopStyleColor();
			ImGui::SameLine();

			if (m_pickMode == 2) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button("R##pickModeRot", ImVec2(17, 17)) && m_pickMode != 2) {
				m_pickMode = 2;
				m_gizmo.SetMode(m_pickMode);
			}
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