#include "PreviewUI.h"
#include "PropertyUI.h"
#include "PipelineUI.h"
#include "../Objects/Settings.h"
#include "../Objects/DefaultState.h"
#include "../Objects/SystemVariableManager.h"
#include "../Objects/KeyboardShortcuts.h"
#include <MoonLight/Base/GeometryFactory.h>
#include <imgui/imgui_internal.h>

#define STATUSBAR_HEIGHT 25 * Settings::Instance().DPIScale
#define BUTTON_SIZE 17 * Settings::Instance().DPIScale
#define FPS_UPDATE_RATE 0.3f
#define BOUNDING_BOX_PADDING 0.01f


const char* BOX_VS_CODE = R"(
cbuffer cbPerFrame : register(b0) {
	float4x4 matWVP;
	float3 color;
};

struct VSInput {
	float3 Position : POSITION;
};

struct VSOutput {
	float4 Position : SV_POSITION;
	float3 Color : COLOR;
};

VSOutput main(VSInput vin) {
	VSOutput vout = (VSOutput)0;
	vout.Position = mul(float4(vin.Position, 1.0f), matWVP);
	vout.Color = color;
	return vout;
})";

const char* BOX_PS_CODE = R"(
struct PSInput
{
	float4 Position : SV_POSITION;
	float3 Color : COLOR;
};

float4 main(PSInput pin) : SV_TARGET
{
	return float4(pin.Color, 1.0f);
})";

namespace ed
{
	DirectX::XMFLOAT3 operator-(const DirectX::XMFLOAT3& left, const DirectX::XMFLOAT3& right)
	{
		return DirectX::XMFLOAT3(left.x - right.x, left.y - right.y, left.z - right.z);
	}
	DirectX::XMFLOAT3 operator*(const DirectX::XMFLOAT3& left, const DirectX::XMFLOAT3& right)
	{
		return DirectX::XMFLOAT3(left.x * right.x, left.y * right.y, left.z * right.z);
	}
	bool operator!=(const DirectX::XMFLOAT3& left, const DirectX::XMFLOAT3& right)
	{
		return (left.x != right.x || left.y != right.y || left.z != right.z);
	}

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
			if (m_picks.size() != 0 && m_hasFocus) {
				PropertyUI* props = (PropertyUI*)m_ui->Get(ViewID::Properties);
				for (int i = 0; i < m_picks.size(); i++) {
					if (props->CurrentItemName() == m_picks[i]->Name)
						props->Open(nullptr);
					m_data->Pipeline.Remove(m_picks[i]->Name);
				}
				m_picks.clear();
			}
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.Unselect", [=]() {
			m_picks.clear();
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.Duplicate", [=]() {
			Duplicate();
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.SelectAll", [=]() {
			// clear the list
			Pick(nullptr);

			// select all geometry and mesh items
			std::vector<PipelineItem*>& pass = m_data->Pipeline.GetList();
			for (int i = 0; i < pass.size(); i++) {
				ed::pipe::ShaderPass* pdata = (ed::pipe::ShaderPass*)pass[i]->Data;
				for (int j = 0; j < pdata->Items.size(); j++) {
					if (pdata->Items[j]->Type == PipelineItem::ItemType::Geometry ||
						pdata->Items[j]->Type == PipelineItem::ItemType::OBJModel)
						Pick(pdata->Items[j], true);
				}
			}
		});
	}
	void PreviewUI::m_setupBoundingBox() {
		ml::Window* wnd = m_data->GetOwner();

		m_boxInput.Add("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0);
		m_boxVS.InputSignature = &m_boxInput;

		m_boxVS.LoadFromMemory(*wnd, BOX_VS_CODE, strlen(BOX_VS_CODE), "main");
		m_boxPS.LoadFromMemory(*wnd, BOX_PS_CODE, strlen(BOX_PS_CODE), "main");

		m_vbBoundingBox.Create(*wnd, 12 * 2);

		m_cbBox.Create(*wnd, sizeof(m_cbBoxData));
	}
	void PreviewUI::OnEvent(const ml::Event & e)
	{
		if (e.Type == ml::EventType::MouseButtonPress)
			m_mouseContact = ImVec2(e.MouseButton.Position.x, e.MouseButton.Position.y);
		else if (e.Type == ml::EventType::MouseMove && Settings::Instance().Preview.Gizmo && m_picks.size() != 0) {
			DirectX::XMFLOAT2 s = SystemVariableManager::Instance().GetMousePosition();
			s.x *= m_lastSize.x;
			s.y *= m_lastSize.y;
			m_gizmo.HandleMouseMove(s.x, s.y, m_lastSize.x, m_lastSize.y);
		}
		else if (e.Type == ml::EventType::MouseButtonRelease && Settings::Instance().Preview.Gizmo)
			m_gizmo.UnselectAxis();
	}
	void PreviewUI::Pick(PipelineItem* item, bool add)
	{
		// reset variables
		m_prevScale = m_tempScale = DirectX::XMFLOAT3(1, 1, 1);
		m_prevRota = m_tempRota = DirectX::XMFLOAT3(0, 0, 0);

		// check if it already exists
		bool skipAdd = false;
		for (int i = 0; i < m_picks.size(); i++)
			if (m_picks[i] == item) {
				if (!add) {
					m_picks.clear();
					m_picks.push_back(item);
				}
				skipAdd = true;
				break;
			}

		// add item
		if (!skipAdd) {
			if (item == nullptr)
				m_picks.clear();
			else if (add)
				m_picks.push_back(item);
			else {
				m_picks.clear();
				m_picks.push_back(item);
			}
		}

		// calculate position
		m_prevTrans = DirectX::XMFLOAT3(0, 0, 0);
		for (int i = 0; i < m_picks.size(); i++) {
			DirectX::XMFLOAT3 pos(0,0,0);
			if (m_picks[i]->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* geo = (pipe::GeometryItem*)m_picks[i]->Data;
				pos = geo->Position;
			}
			else if (m_picks[i]->Type == PipelineItem::ItemType::OBJModel) {
				pipe::OBJModel* obj = (pipe::OBJModel*)m_picks[i]->Data;
				pos = obj->Position;
			}
			m_prevTrans.x += pos.x;
			m_prevTrans.y += pos.y;
			m_prevTrans.z += pos.z;
		}
		if (m_picks.size() != 0) {
			m_prevTrans.x /= m_picks.size();
			m_prevTrans.y /= m_picks.size();
			m_prevTrans.z /= m_picks.size();
		}
		m_tempTrans = m_prevTrans;


		if (m_picks.size() != 0) {
			if (Settings::Instance().Preview.BoundingBox)
				m_buildBoundingBox();

			m_gizmo.SetTransform(&m_tempTrans, &m_tempScale, &m_tempRota);
		}
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
		if (m_picks.size() != 0 && settings.Preview.Gizmo) {
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

			if (settings.Preview.BoundingBox)
				m_renderBoundingBox();

			m_data->GetOwner()->Bind();

			ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMin().y);
			ImGui::Image(m_rtView.GetView(), imageSize);
		}

		// update wasd key state
		SystemVariableManager::Instance().SetKeysWASD(GetAsyncKeyState('W') != 0, GetAsyncKeyState('A') != 0, GetAsyncKeyState('S') != 0, GetAsyncKeyState('D') != 0);

		// update system variable mouse position value
		SystemVariableManager::Instance().SetMousePosition((ImGui::GetMousePos().x - ImGui::GetCursorScreenPos().x - ImGui::GetScrollX()) / imageSize.x,
			(imageSize.y + (ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y - ImGui::GetScrollY())) / imageSize.y);

		// apply transformations to picked items
		if (settings.Preview.Gizmo && m_picks.size() != 0 && 
			(m_tempTrans != m_prevTrans || m_tempScale != m_prevScale || m_tempRota != m_prevRota)) {
			for (int i = 0; i < m_picks.size(); i++) {
				// TODO: tidy this up
				DirectX::XMFLOAT3 t = m_tempTrans - m_prevTrans;
				DirectX::XMFLOAT3 s = m_tempScale - m_prevScale;
				DirectX::XMFLOAT3 r = m_tempRota - m_prevRota;
				DirectX::XMFLOAT3* ot = nullptr, *os = nullptr, *orot = nullptr;

				if (m_picks[i]->Type == PipelineItem::ItemType::Geometry) {
					pipe::GeometryItem* geo = (pipe::GeometryItem*)m_picks[i]->Data;
					ot = &geo->Position;
					os = &geo->Scale;
					orot = &geo->Rotation;
				}
				else if (m_picks[i]->Type == PipelineItem::ItemType::OBJModel) {
					pipe::OBJModel* obj = (pipe::OBJModel*)m_picks[i]->Data;
					ot = &obj->Position;
					os = &obj->Scale;
					orot = &obj->Rotation;
				}

				if (ot != nullptr) {
					ot->x += t.x;
					ot->y += t.y;
					ot->z += t.z;
				}
				if (os != nullptr) {
					os->x += s.x;
					os->y += s.y;
					os->z += s.z;
				}
				if (orot != nullptr) {
					orot->x += r.x;
					orot->y += r.y;
					orot->z += r.z;
				}

			}

			m_prevTrans = m_tempTrans;
			m_prevRota = m_tempRota;
			m_prevScale = m_tempScale;

			m_buildBoundingBox();
		}

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

				if ((m_picks.size() != 0 && m_gizmo.Click(s.x, s.y, m_lastSize.x, m_lastSize.y) == -1) || m_picks.size() == 0) {
					renderer->Pick(s.x, s.y, [&](PipelineItem* item) {
						if (settings.Preview.PropertyPick)
							((PropertyUI*)m_ui->Get(ViewID::Properties))->Open(item);

						auto shift = ImGui::GetIO().KeyShift;
						Pick(item, shift);
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
			if (ImGui::Button("P##pickModePos", ImVec2(BUTTON_SIZE, BUTTON_SIZE)) && m_pickMode != 0) {
				m_pickMode = 0;
				m_gizmo.SetMode(m_pickMode);
			}
			else if (m_pickMode == 0) ImGui::PopStyleColor();
			ImGui::SameLine();

			if (m_pickMode == 1) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button("S##pickModeScl", ImVec2(BUTTON_SIZE, BUTTON_SIZE)) && m_pickMode != 1) {
				m_pickMode = 1;
				m_gizmo.SetMode(m_pickMode);
			}
			else if (m_pickMode == 1) ImGui::PopStyleColor();
			ImGui::SameLine();

			if (m_pickMode == 2) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button("R##pickModeRot", ImVec2(BUTTON_SIZE, BUTTON_SIZE)) && m_pickMode != 2) {
				m_pickMode = 2;
				m_gizmo.SetMode(m_pickMode);
			}
			else if (m_pickMode == 2) ImGui::PopStyleColor();
			ImGui::SameLine();

			if (m_picks.size() != 0) {
				ImGui::Text("|");
				ImGui::SameLine();
				ImGui::Text("Picked: ");
				
				for (int i = 0; i < m_picks.size(); i++) {
					ImGui::SameLine();

					if (i != m_picks.size() - 1)
						ImGui::Text("%s,", m_picks[i]->Name);
					else
						ImGui::Text("%s", m_picks[i]->Name);
				}
			}
		}
	}
	void PreviewUI::Duplicate()
	{
		if (m_picks.size() == 0)
			return;

		// store pointers to these objects as we will select them after duplication
		std::vector<PipelineItem*> duplicated;

		// duplicate each item
		for (int i = 0; i < m_picks.size(); i++) {
			ed::PipelineItem* item = m_picks[i];

			// first find a name that is not used
			std::string name = std::string(item->Name);
		
			// remove numbers at the end of the string
			size_t lastOfLetter = std::string::npos;
			for (size_t j = name.size()-1; j > 0; j--)
				if (!std::isdigit(name[j])) {
					lastOfLetter = j + 1;
					break;
				}
			if (lastOfLetter != std::string::npos)
				name = name.substr(0, lastOfLetter);

			// add number to the string and check if it already exists
			for (size_t j = 2; /*WE WILL BRAKE FROM INSIDE ONCE WE FIND THE NAME*/;j++) {
				std::string newName = name + std::to_string(j);
				bool has = m_data->Pipeline.Has(newName.c_str());

				if (!has) {
					name = newName;
					break;
				}
			}

			// get item owner
			char* owner = m_data->Pipeline.GetItemOwner(item->Name);

			// once we found a name, duplicate the properties:
			// duplicate geometry object:
			if (item->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* data = new pipe::GeometryItem();
				pipe::GeometryItem* origData = (pipe::GeometryItem*)item->Data;

				data->Position = origData->Position;
				data->Rotation = origData->Rotation;
				data->Scale = origData->Scale;
				data->Size = origData->Size;
				data->Topology = origData->Topology;
				data->Type = origData->Type;

				if (data->Type == pipe::GeometryItem::GeometryType::Cube)
					data->Geometry = ml::GeometryFactory::CreateCube(data->Size.x, data->Size.y, data->Size.z, *m_data->GetOwner());
				else if (data->Type == pipe::GeometryItem::Circle) {
					data->Geometry = ml::GeometryFactory::CreateCircle(0, 0, data->Size.x, data->Size.y, *m_data->GetOwner());
					data->Topology = ml::Topology::TriangleStrip;
				}
				else if (data->Type == pipe::GeometryItem::Plane)
					data->Geometry = ml::GeometryFactory::CreatePlane(data->Size.x, data->Size.y, *m_data->GetOwner());
				else if (data->Type == pipe::GeometryItem::Rectangle)
					data->Geometry = ml::GeometryFactory::CreatePlane(1, 1, *m_data->GetOwner());
				else if (data->Type == pipe::GeometryItem::Sphere)
					data->Geometry = ml::GeometryFactory::CreateSphere(data->Size.x, *m_data->GetOwner());
				else if (data->Type == pipe::GeometryItem::Triangle)
					data->Geometry = ml::GeometryFactory::CreateTriangle(0, 0, data->Size.x, *m_data->GetOwner());

				m_data->Pipeline.AddItem(owner, name.c_str(), item->Type, data);
			}

			// duplicate OBJModel:
			else if (item->Type == PipelineItem::ItemType::OBJModel) {
				pipe::OBJModel* data = new pipe::OBJModel();
				pipe::OBJModel* origData = (pipe::OBJModel*)item->Data;

				strcpy(data->Filename, origData->Filename);
				strcpy(data->GroupName, origData->GroupName);
				data->OnlyGroup = origData->OnlyGroup;
				data->Scale = origData->Scale;
				data->Position = origData->Position;
				data->Rotation = origData->Rotation;


				if (strlen(data->Filename) > 0) {
					std::string objMem = m_data->Parser.LoadProjectFile(data->Filename);
					ml::OBJModel* mdl = m_data->Parser.LoadModel(data->Filename);

					bool loaded = mdl != nullptr;
					if (loaded)
						data->Mesh = *mdl;
					else m_data->Messages.Add(ed::MessageStack::Type::Error, owner, "Failed to create .obj model " + std::string(item->Name));

					// TODO: if (!loaded) error "Failed to load a mesh"

					if (loaded) {
						ml::OBJModel::Vertex* verts = data->Mesh.GetVertexData();
						ml::UInt32 vertCount = data->Mesh.GetVertexCount();

						if (data->OnlyGroup) {
							verts = data->Mesh.GetGroupVertices(data->GroupName);
							vertCount = data->Mesh.GetGroupVertexCount(data->GroupName);

							if (verts == nullptr) {
								verts = data->Mesh.GetObjectVertices(data->GroupName);
								vertCount = data->Mesh.GetObjectVertexCount(data->GroupName);

								// TODO: if (verts == nullptr) error "failed to find a group with that name"
							}
						}

						data->VertCount = vertCount;
						data->Vertices.Create(*m_data->GetOwner(), verts, vertCount, ml::Resource::Immutable);
					}
					// TODO: else error "failed to load"
				}

				m_data->Pipeline.AddItem(owner, name.c_str(), item->Type, data);
			}


			duplicated.push_back(m_data->Pipeline.Get(name.c_str()));
		}

		// select the newly created items
		m_picks = duplicated;
	}
	
	void PreviewUI::m_buildBoundingBox()
	{
		DirectX::XMFLOAT3 minPos(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()),
			maxPos(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());

		// find min and max pos
		for (int i = 0; i < m_picks.size(); i++) {
			ed::PipelineItem* item = m_picks[i];
			DirectX::XMFLOAT3 minPosItem(0,0,0), maxPosItem(0,0,0);
			DirectX::XMVECTOR rota = DirectX::XMVectorSet(0,0,0,0);
			DirectX::XMVECTOR pos = DirectX::XMVectorSet(0, 0, 0, 0);
			if (item->Type == ed::PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* data = (pipe::GeometryItem*)item->Data;
				DirectX::XMFLOAT3 size(data->Size.x * data->Scale.x, data->Size.y * data->Scale.y, data->Size.z * data->Scale.z);
				
				minPosItem = DirectX::XMFLOAT3(- size.x / 2, - size.y / 2, - size.z / 2);
				maxPosItem = DirectX::XMFLOAT3(+ size.x / 2, + size.y / 2, + size.z / 2);

				rota = DirectX::XMLoadFloat3(&data->Rotation);
				pos = DirectX::XMLoadFloat3(&data->Position);
			}
			else if (item->Type == ed::PipelineItem::ItemType::OBJModel) {
				pipe::OBJModel* model = (pipe::OBJModel*)item->Data;
				ml::Bounds3D bounds = model->Mesh.GetBounds();
				minPosItem = bounds.Min * model->Scale; // TODO: add positions so that it works for multiple objects
				maxPosItem = bounds.Max * model->Scale;

				rota = DirectX::XMLoadFloat3(&model->Rotation);
				pos = DirectX::XMLoadFloat3(&model->Position);
			}

			// 8 points
			float pointsX[8] = { minPosItem.x, minPosItem.x, minPosItem.x, minPosItem.x, maxPosItem.x, maxPosItem.x, maxPosItem.x, maxPosItem.x };
			float pointsY[8] = { minPosItem.y, minPosItem.y, maxPosItem.y, maxPosItem.y, minPosItem.y, minPosItem.y, maxPosItem.y, maxPosItem.y };
			float pointsZ[8] = { minPosItem.z, maxPosItem.z, minPosItem.z, maxPosItem.z, minPosItem.z, maxPosItem.z, minPosItem.z, maxPosItem.z };
			
			// apply rotation and translation to those 8 points and check for min and max pos
			for (int j = 0; j < 8; j++) {
				DirectX::XMVECTOR pointVec = DirectX::XMVectorSet(pointsX[j], pointsY[j], pointsZ[j], 0);
				pointVec = DirectX::XMVector3Transform(pointVec,
					DirectX::XMMatrixRotationRollPitchYawFromVector(rota) * DirectX::XMMatrixTranslationFromVector(pos));

				DirectX::XMFLOAT3 point;
				DirectX::XMStoreFloat3(&point, pointVec);

				minPos.x = std::min<float>(point.x, minPos.x);
				minPos.y = std::min<float>(point.y, minPos.y);
				minPos.z = std::min<float>(point.z, minPos.z);
				maxPos.x = std::max<float>(point.x, maxPos.x);
				maxPos.y = std::max<float>(point.y, maxPos.y);
				maxPos.z = std::max<float>(point.z, maxPos.z);
			}
		}

		minPos.x -= BOUNDING_BOX_PADDING; minPos.y -= BOUNDING_BOX_PADDING; minPos.z -= BOUNDING_BOX_PADDING;
		maxPos.x += BOUNDING_BOX_PADDING; maxPos.y += BOUNDING_BOX_PADDING; maxPos.z += BOUNDING_BOX_PADDING;

		// lines
		std::vector<QuadVertex2D> verts = {
			// back face
			{ DirectX::XMFLOAT4(minPos.x, minPos.y, minPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, minPos.y, minPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, minPos.y, minPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, maxPos.y, minPos.z, 0) },
			{ DirectX::XMFLOAT4(minPos.x, minPos.y, minPos.z, 0) },
			{ DirectX::XMFLOAT4(minPos.x, maxPos.y, minPos.z, 0) },
			{ DirectX::XMFLOAT4(minPos.x, maxPos.y, minPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, maxPos.y, minPos.z, 0) },

			// front face
			{ DirectX::XMFLOAT4(minPos.x, minPos.y, maxPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, minPos.y, maxPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, minPos.y, maxPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, maxPos.y, maxPos.z, 0) },
			{ DirectX::XMFLOAT4(minPos.x, minPos.y, maxPos.z, 0) },
			{ DirectX::XMFLOAT4(minPos.x, maxPos.y, maxPos.z, 0) },
			{ DirectX::XMFLOAT4(minPos.x, maxPos.y, maxPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, maxPos.y, maxPos.z, 0) },

			// sides
			{ DirectX::XMFLOAT4(minPos.x, minPos.y, minPos.z, 0) },
			{ DirectX::XMFLOAT4(minPos.x, minPos.y, maxPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, minPos.y, minPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, minPos.y, maxPos.z, 0) },
			{ DirectX::XMFLOAT4(minPos.x, maxPos.y, minPos.z, 0) },
			{ DirectX::XMFLOAT4(minPos.x, maxPos.y, maxPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, maxPos.y, minPos.z, 0) },
			{ DirectX::XMFLOAT4(maxPos.x, maxPos.y, maxPos.z, 0) },
		};
		m_vbBoundingBox.Update(verts.data());

		// calculate color for the bounding box
		ml::Color clearClr = Settings::Instance().Project.ClearColor;
		float avgClear = (((float)clearClr.R + clearClr.G + clearClr.B) / 3.0f) / 255.0f;
		ImVec4 wndBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
		float avgWndBg = (wndBg.x + wndBg.y + wndBg.z) / 3;
		float clearAlpha = clearClr.A / 255.0f;
		float color = avgClear * clearAlpha + avgWndBg * (1 - clearAlpha);
		if (color >= 0.5f)
			m_cbBoxData.color = DirectX::XMFLOAT3(0, 0, 0);
		else
			m_cbBoxData.color = DirectX::XMFLOAT3(1, 1, 1);
	}
	void PreviewUI::m_renderBoundingBox()
	{
		DirectX::XMMATRIX matWorld = DirectX::XMMatrixIdentity();
		DirectX::XMMATRIX matProj = matWorld * SystemVariableManager::Instance().GetViewMatrix() * SystemVariableManager::Instance().GetProjectionMatrix();
		DirectX::XMStoreFloat4x4(&m_cbBoxData.matWVP, DirectX::XMMatrixTranspose(matProj));
		m_cbBox.Update(&m_cbBoxData);

		m_data->GetOwner()->SetTopology(ml::Topology::LineList);
		m_boxVS.Bind();
		m_boxPS.Bind();
		m_data->GetOwner()->SetInputLayout(m_boxInput);
		m_vbBoundingBox.Bind();
		m_cbBox.BindVS();
		m_data->GetOwner()->Draw(24);
	}
}