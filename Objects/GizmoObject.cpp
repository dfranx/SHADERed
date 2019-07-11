#include "GizmoObject.h"
#include "SystemVariableManager.h"
#include "../Objects/DefaultState.h"
#include <MoonLight/Base/GeometryFactory.h>
#include <DirectXCollision.h>

const char* GIZMO_VS_CODE = R"(
cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matWorld;
};

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float3 Color : COLOR;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

static const float3 light1 = float3(1, 1, 1);
static const float3 light2 = float3(-1, 1, -1);
static const float lightRatio = 0.8f;
VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;
	
	vin.Normal = normalize(mul(float4(vin.Normal, 0), matWorld));
	vout.Position = mul(float4(vin.Position, 1.0f), matWorld);
	
	float3 lightVec1 = normalize(light1);
	float3 lightVec2 = normalize(light2);
	
	float lightFactor = saturate(dot(lightVec1, vin.Normal)) + saturate(dot(lightVec2, vin.Normal));
	float3 clr = vin.Color*(1-lightRatio) + lightFactor*vin.Color*lightRatio;

	vout.Color = float4(clr, 1.0f);
	vout.Position = mul(vout.Position, matVP);
	
	return vout;
})";

const char* GIZMO_PS_CODE = R"(
struct PSInput
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

float4 main(PSInput pin) : SV_TARGET
{
	return pin.Color;
})";
const char* GUI_VS_CODE = R"(
cbuffer cbPerFrame : register(b0) {
	float4x4 matWVP;
};

struct VSInput {
	float3 Position : POSITION;
};

struct VSOutput {
	float4 Position : SV_POSITION;
};

VSOutput main(VSInput vin) {
	VSOutput vout = (VSOutput)0;
	vout.Position = mul(float4(vin.Position, 1.0f), matWVP);
	return vout;
})";

const char* GUI_PS_CODE = R"(
struct PSInput
{
	float4 Position : SV_POSITION;
};

float4 main(PSInput pin) : SV_TARGET
{
	return float4(1,0.84313f,0, 0.51f);
})";
#define GIZMO_SCALE_FACTOR 7.5f
#define GIZMO_HEIGHT 1.1f
#define GIZMO_WIDTH 0.05f
#define GIZMO_POINTER_WIDTH 0.1f
#define GIZMO_POINTER_HEIGHT 0.175f
#define GIZMO_PRECISE_COLBOX_WD 2.0f // increase the collision box by 100% in width and depth
#define GIZMO_SELECTED_COLOR DirectX::XMFLOAT3(1,0.84313f,0)

namespace ed
{
	ml::Geometry::Vertex* CreateDegreeInfo(float degreesStart, float degreesEnd, size_t& count, size_t pointCount)
	{
		int x = 0, y = 0;
		float radius = 115;

		count = pointCount * 2 + 1;
		ml::Geometry::Vertex* ret = new ml::Geometry::Vertex[count];

		float degrees = degreesEnd - degreesStart;
		if (degrees < 0) degrees += 360;

		float step = DirectX::XMConvertToRadians(degrees) / pointCount;
		float radStart = DirectX::XMConvertToRadians(degreesStart);

		ret[0].Position = DirectX::XMFLOAT3(x + radius * sin(radStart), y + radius * cos(radStart), 0);
		ret[1].Position = DirectX::XMFLOAT3(x + radius * sin(radStart+step), y + radius * cos(radStart + step), 0);
		ret[2].Position = DirectX::XMFLOAT3(x, y, 0);

		ret[0].UV = DirectX::XMFLOAT2(sin(0) * 0.5f + 0.5f, 1 - (cos(0) * 0.5f + 0.5f));
		ret[1].UV = DirectX::XMFLOAT2(sin(step) * 0.5f + 0.5f, 1 - (cos(step) * 0.5f + 0.5f));
		ret[2].UV = DirectX::XMFLOAT2(0.5f, 0.5f);

		ret[0].Normal = DirectX::XMFLOAT3(0, 0, -1);
		ret[1].Normal = DirectX::XMFLOAT3(0, 0, -1);
		ret[2].Normal = DirectX::XMFLOAT3(0, 0, -1);

		for (int i = 0; i < pointCount - 1; i++) {
			float xVal = sin(radStart + step * (i + 2));
			float yVal = cos(radStart + step * (i + 2));
			ret[i * 2 + 3].Position = DirectX::XMFLOAT3(x + radius * xVal, y + radius * yVal, 0);
			ret[i * 2 + 4].Position = DirectX::XMFLOAT3(x, y, 0);

			ret[i * 2 + 3].UV = DirectX::XMFLOAT2(xVal * 0.5f + 0.5f, 1 - (yVal * 0.5f + 0.5f));
			ret[i * 2 + 4].UV = DirectX::XMFLOAT2(0.5f, 0.5f);

			ret[i * 2 + 3].Normal = DirectX::XMFLOAT3(0, 0, -1);
			ret[i * 2 + 4].Normal = DirectX::XMFLOAT3(0, 0, -1);
		}

		return ret;
	}
	ml::Geometry CreateDegreeInfo(float degreesStart, float degreesEnd, ml::Window& wnd, size_t pointCount)
	{
		ml::Geometry ret;
		size_t vertCount = 0;

		ml::Geometry::Vertex* verts = CreateDegreeInfo(degreesStart, degreesEnd, vertCount, pointCount);
		ret.Create(wnd, verts, vertCount);
		delete[] verts;

		return ret;
	}


	GizmoObject::GizmoObject(ml::Window* wnd) :
		m_wnd(wnd),
		m_axisSelected(-1), m_vw(-1), m_vh(-1),
		m_mode(0)
	{
		m_vlayout.Add("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0);
		m_vlayout.Add("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 12);
		m_vlayout.Add("COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 24);
		m_vs.InputSignature = &m_vlayout;

		m_uiInput.Add("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0);
		m_uiPS.LoadFromMemory(*wnd, GUI_PS_CODE, strlen(GUI_PS_CODE), "main");
		m_uiVS.InputSignature = &m_uiInput;
		m_uiVS.LoadFromMemory(*wnd, GUI_VS_CODE, strlen(GUI_VS_CODE), "main");
		m_cbUI.Create(*wnd, &m_cbUIData, sizeof(CBDegreeUI));

		m_degreeInfoUI = CreateDegreeInfo(45, 90, *wnd, 24);
		
		m_rasterState.Info.DepthClipEnable = false;
		m_rasterState.Info.CullMode = D3D11_CULL_NONE;
		m_rasterState.Create(*wnd);
		m_ignoreDepth.Info.DepthEnable = false;
		m_ignoreDepth.Info.DepthFunc = D3D11_COMPARISON_ALWAYS;
		m_ignoreDepth.Create(*wnd);
		m_transparencyBlend.Info.IndependentBlendEnable = true;
		m_transparencyBlend.Info.AlphaToCoverageEnable = true;
		m_transparencyBlend.Info.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		m_transparencyBlend.Info.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		m_transparencyBlend.Info.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
		m_transparencyBlend.Create(*wnd);


		m_vs.LoadFromMemory(*m_wnd, GIZMO_VS_CODE, strlen(GIZMO_VS_CODE), "main");
		m_ps.LoadFromMemory(*m_wnd, GIZMO_PS_CODE, strlen(GIZMO_PS_CODE), "main");

		m_model.LoadFromFile("data/gizmo.obj");

		m_cb.Create(*m_wnd, &m_cbData, sizeof(m_cbData));

		m_vertsCreated = false;
		m_axisHovered = -1;

		m_colors[0] = DirectX::XMFLOAT3(1, 0, 0);
		m_colors[1] = DirectX::XMFLOAT3(0, 1, 0);
		m_colors[2] = DirectX::XMFLOAT3(0, 0, 1);
		m_buildHandles();
	}
	GizmoObject::~GizmoObject()
	{
	}
	void GizmoObject::HandleMouseMove(int x, int y, int vw, int vh)
	{
		// handle dragging rotation controls
		if (m_axisSelected != -1 && m_mode == 2) {
			// update UI cb
			if (Settings::Instance().Preview.GizmoRotationUI) {
				DirectX::XMMATRIX matProj = DirectX::XMMatrixOrthographicLH(m_vw, m_vh, 0.1f, 1000.0f);
				DirectX::XMStoreFloat4x4(&m_cbUIData.matWVP, DirectX::XMMatrixTranspose(DirectX::XMMatrixRotationZ(DirectX::XM_PI) *
					DirectX::XMMatrixRotationY(DirectX::XM_PI) *
					matProj));
				m_cbUI.Update(&m_cbUIData);
			}

			// rotate the object
			float degNow = DirectX::XMConvertToDegrees(atan2(x - (vw / 2), y - (vh / 2)));
			float deg = degNow-m_clickDegrees;

			if (Settings::Instance().Preview.GizmoRotationUI)
				m_degreeInfoUI = CreateDegreeInfo(m_clickDegrees, degNow, *m_wnd, 24);

			if (deg < 0) deg = 360 + deg;

			float rad = deg / 180 * DirectX::XM_PI;
			switch (m_axisSelected) {
			case 0: m_rota->x = rad; break;
			case 1: m_rota->y = rad; break;
			case 2: m_rota->z = rad; break;
			}
		}

		// handle hovering over controls
		m_axisHovered = -1;
		m_vw = vw;
		m_vh = vh;

		float scale = DirectX::XMVectorGetX(
			DirectX::XMVector3Length(
				DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(m_trans), SystemVariableManager::Instance().GetCamera()->GetPosition()
				))) / GIZMO_SCALE_FACTOR;

		// X axis
		DirectX::XMMATRIX xWorld = DirectX::XMMatrixRotationZ(DirectX::XM_PIDIV2) *
			DirectX::XMMatrixTranslation(m_trans->x + scale * (GIZMO_HEIGHT / 2 + GIZMO_WIDTH / 2), m_trans->y, m_trans->z);

		// Y axis
		DirectX::XMMATRIX yWorld = DirectX::XMMatrixTranslation(m_trans->x, m_trans->y + scale * (GIZMO_HEIGHT / 2 - GIZMO_WIDTH / 2), m_trans->z);

		// Z axis
		DirectX::XMMATRIX zWorld = DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2) *
			DirectX::XMMatrixTranslation(m_trans->x, m_trans->y, m_trans->z + scale * (GIZMO_HEIGHT / 2 + GIZMO_WIDTH / 2));

		DirectX::XMMATRIX proj = SystemVariableManager::Instance().GetProjectionMatrix();
		DirectX::XMFLOAT4X4 proj4x4; DirectX::XMStoreFloat4x4(&proj4x4, proj);

		float vx = (+2.0f * x / m_vw - 1.0f) / proj4x4(0, 0);
		float vy = (-2.0f * y / m_vh + 1.0f) / proj4x4(1, 1);

		DirectX::XMVECTOR rayOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		DirectX::XMVECTOR rayDir = DirectX::XMVectorSet(vx, vy, 1.0f, 0.0f);

		DirectX::XMMATRIX view = SystemVariableManager::Instance().GetCamera()->GetMatrix();
		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&XMMatrixDeterminant(view), view);

		rayOrigin = DirectX::XMVector3TransformCoord(rayOrigin, invView);
		rayDir = DirectX::XMVector3TransformNormal(rayDir, invView);

		if (m_mode == 0 || m_mode == 1) {
			DirectX::BoundingBox handleBox;
			handleBox.Center = DirectX::XMFLOAT3(0, 0, 0);
			handleBox.Extents = DirectX::XMFLOAT3(GIZMO_WIDTH * GIZMO_PRECISE_COLBOX_WD * scale / 2, (GIZMO_HEIGHT + GIZMO_POINTER_HEIGHT) * scale / 2, GIZMO_WIDTH * GIZMO_PRECISE_COLBOX_WD * scale / 2);

			// X axis
			DirectX::XMMATRIX invWorld = DirectX::XMMatrixInverse(&XMMatrixDeterminant(xWorld), xWorld);
			DirectX::XMVECTOR rayOrigin2 = DirectX::XMVector3TransformCoord(rayOrigin, invWorld);
			DirectX::XMVECTOR rayDir2 = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(rayDir, invWorld));

			float distX, distY, distZ, dist = std::numeric_limits<float>::infinity();
			if (handleBox.Intersects(rayOrigin2, rayDir2, distX))
				m_axisHovered = 0, dist = distX;
			else distX = std::numeric_limits<float>::infinity();

			// Y axis
			invWorld = DirectX::XMMatrixInverse(&XMMatrixDeterminant(yWorld), yWorld);
			rayOrigin2 = DirectX::XMVector3TransformCoord(rayOrigin, invWorld);
			rayDir2 = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(rayDir, invWorld));

			if (handleBox.Intersects(rayOrigin2, rayDir2, distY) && distY < distX)
				m_axisHovered = 1, dist = distY;
			else distY = std::numeric_limits<float>::infinity();

			// Z axis
			invWorld = DirectX::XMMatrixInverse(&XMMatrixDeterminant(zWorld), zWorld);
			rayOrigin2 = DirectX::XMVector3TransformCoord(rayOrigin, invWorld);
			rayDir2 = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(rayDir, invWorld));

			if (handleBox.Intersects(rayOrigin2, rayDir2, distZ) && distZ < distY && distZ < distX)
				m_axisHovered = 2, dist = distZ;
			else distZ = std::numeric_limits<float>::infinity();

			if (m_axisHovered != -1) {
				m_hoverDepth = dist;
				m_hoverStart = DirectX::XMVectorAdd(rayOrigin, DirectX::XMVectorScale(rayDir, m_hoverDepth));
			}
		}
		else { // handle the rotation bars
			DirectX::XMMATRIX matWorld = DirectX::XMMatrixScaling(scale, scale, scale) *
				DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2) *
				DirectX::XMMatrixTranslation(m_trans->x, m_trans->y, m_trans->z);
			DirectX::XMMATRIX invWorld = DirectX::XMMatrixInverse(&XMMatrixDeterminant(matWorld), matWorld);
			DirectX::XMVECTOR rayOrigin2 = DirectX::XMVector3TransformCoord(rayOrigin, invWorld);
			DirectX::XMVECTOR rayDir2 = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(rayDir, invWorld));

			float triDist = 0;
			float curDist = std::numeric_limits<float>::infinity();
			int selIndex = -1;
			for (int i = 0; i < m_verts.size(); i += 3) {
				DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&m_verts[i + 0].Position);
				DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&m_verts[i + 1].Position);
				DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&m_verts[i + 2].Position);

				if (DirectX::TriangleTests::Intersects(rayOrigin2, rayDir2, v0, v1, v2, triDist)) {
					if (triDist < curDist) {
						curDist = triDist;
						selIndex = i;
					}
				}
			}

			if (selIndex != -1) {
				m_axisHovered = selIndex / (m_verts.size() / 3);
				m_hoverDepth = curDist;
				m_hoverStart = DirectX::XMVectorAdd(rayOrigin, DirectX::XMVectorScale(rayDir, m_hoverDepth));
			}
		}

		// update vb
		if (m_axisSelected == -1) {
			int bounds = m_verts.size() / 3;
			for (int i = 0; i < m_verts.size(); i++)
				m_verts[i].Color = (i / bounds == m_axisHovered) ? GIZMO_SELECTED_COLOR : m_colors[i / bounds];
			m_buffer.Update(m_verts.data());
		}
	}
	int GizmoObject::Click(int x, int y, int vw, int vh)
	{
		// handle control selection
		m_axisSelected = m_axisHovered;
		if (m_axisSelected != -1) {
			m_clickDepth = m_hoverDepth;
			m_clickStart = m_hoverStart;

			if (m_mode == 2) {
				m_clickDegrees = DirectX::XMConvertToDegrees(atan2(x - (vw / 2), y - (vh / 2)));

				if (Settings::Instance().Preview.GizmoRotationUI)
					m_degreeInfoUI = CreateDegreeInfo(0, 0, *m_wnd, 24);
			}
		}

		return m_axisSelected;
	}
	void GizmoObject::Move(int dx, int dy)
	{
		// dont handle the rotation controls here as we handle that in the HandleMouseMove method
		if (m_axisSelected == -1 || m_mode == 2)
			return;

		// handle scaling and translating
		DirectX::XMMATRIX proj = SystemVariableManager::Instance().GetProjectionMatrix();
		DirectX::XMFLOAT4X4 proj4x4; DirectX::XMStoreFloat4x4(&proj4x4, proj);

		float vx = (+2.0f * dx / m_vw - 1.0f) / proj4x4(0, 0);
		float vy = (-2.0f * dy / m_vh + 1.0f) / proj4x4(1, 1);

		DirectX::XMVECTOR rayOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		DirectX::XMVECTOR rayDir = DirectX::XMVectorSet(vx, vy, 1.0f, 0.0f);

		DirectX::XMMATRIX view = SystemVariableManager::Instance().GetCamera()->GetMatrix();
		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&XMMatrixDeterminant(view), view);
		
		DirectX::XMVECTOR axisVec = DirectX::XMVectorSet(m_axisSelected == 0, m_axisSelected == 1, m_axisSelected == 2, 0);
		DirectX::XMVECTOR tAxisVec = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(axisVec, invView));

		rayOrigin = DirectX::XMVector3TransformCoord(rayOrigin, invView);
		rayDir = DirectX::XMVector3TransformNormal(rayDir, invView);

		DirectX::XMVECTOR mouseVec = DirectX::XMVectorAdd(rayOrigin, DirectX::XMVectorScale(rayDir, m_clickDepth));
		DirectX::XMVECTOR moveVec = DirectX::XMVectorSubtract(m_clickStart, mouseVec);
		moveVec = DirectX::XMVector3TransformNormal(moveVec, invView);

		float dotval = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Normalize(moveVec), tAxisVec));
		float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(moveVec));

		float scale = DirectX::XMVectorGetX(
			DirectX::XMVector3Length(
				DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(m_trans), SystemVariableManager::Instance().GetCamera()->GetPosition()
				))) / GIZMO_SCALE_FACTOR;

		float moveDist = -length * dotval * scale;

		if (m_mode == 0) {
			if (m_axisSelected == 0) m_trans->x += moveDist;
			else if (m_axisSelected == 1) m_trans->y += moveDist;
			else if (m_axisSelected == 2) m_trans->z += moveDist;
		} else if (m_mode == 1) {
			if (m_axisSelected == 0) m_scale->x += moveDist;
			else if (m_axisSelected == 1) m_scale->y += moveDist;
			else if (m_axisSelected == 2) m_scale->z += moveDist;
		} else if (m_mode == 2) {
			if (m_axisSelected == 0) m_rota->x += moveDist;
			else if (m_axisSelected == 1) m_rota->y += moveDist;
			else if (m_axisSelected == 2) m_rota->z += moveDist;
		}

		m_clickStart = mouseVec;
	}
	void GizmoObject::Render()
	{
		float scale = DirectX::XMVectorGetX(
			DirectX::XMVector3Length(
				DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(m_trans), SystemVariableManager::Instance().GetCamera()->GetPosition()
				))) / GIZMO_SCALE_FACTOR;

		m_vs.Bind();
		m_ps.Bind();
		m_wnd->SetInputLayout(m_vlayout);
		m_wnd->SetTopology(ml::Topology::TriangleList);

		m_cb.BindVS(0);

		DirectX::XMMATRIX matWorld = DirectX::XMMatrixScaling(scale, scale, scale) *
			DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2)*
			DirectX::XMMatrixTranslation(m_trans->x, m_trans->y, m_trans->z);
		DirectX::XMStoreFloat4x4(&m_cbData.matWorld, DirectX::XMMatrixTranspose(matWorld));
		m_cb.Update(&m_cbData);

		m_buffer.Bind();
		m_wnd->Draw(m_verts.size());


		// degree info UI
		if (m_axisSelected != -1 && m_mode == 2 && Settings::Instance().Preview.GizmoRotationUI)
		{
			m_cbUI.BindVS(0);

			m_ignoreDepth.Bind();
			m_transparencyBlend.Bind();
			m_rasterState.Bind();

			m_wnd->SetTopology(ml::Topology::TriangleStrip);
			m_wnd->SetInputLayout(m_uiInput);
			m_uiPS.Bind();
			m_uiVS.Bind();
			m_degreeInfoUI.Draw();

			DefaultState::Instance().Bind();
		}
	}
	void GizmoObject::m_buildHandles()
	{
		size_t vertsSize = 0;

		std::vector<std::string> objects;
		if (m_mode == 0) // translation
			objects = { "HandleX", "ArrowX", "HandleY", "ArrowY", "HandleZ", "ArrowZ" };
		else if (m_mode == 1) // Scale
			objects = { "HandleX", "CyX", "HandleY", "CyY", "HandleZ", "CyZ" };
		else if (m_mode == 2) // Rotation
			objects = { "RotateX", "RotateY", "RotateZ" };

		int curi = 0;
		for (int i = 0; i < objects.size(); i++) {
			size_t vcnt = m_model.GetObjectVertexCount(objects[i]);
			ml::OBJModel::Vertex* v = m_model.GetObjectVertices(objects[i]);

			vertsSize += vcnt;
			m_verts.resize(vertsSize);

			for (int j = 0; j < vcnt; j++) {
				m_verts[curi + j].Position = v[j].Position;
				m_verts[curi + j].Normal = v[j].Normal;
			}
			curi += vcnt;
		}

		int bounds = vertsSize / 3;
		for (int i = 0; i < vertsSize; i++)
			m_verts[i].Color = m_colors[i / bounds];

		if (m_vertsCreated)
			m_buffer.Release();

		m_buffer.Create(*m_wnd, m_verts.data(), m_verts.size());
		
		m_vertsCreated = true;
	}
}