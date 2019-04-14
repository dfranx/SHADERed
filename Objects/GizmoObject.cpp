#include "GizmoObject.h"
#include "SystemVariableManager.h"
#include <MoonLight/Base/GeometryFactory.h>
#include <DirectXCollision.h>

const char* GIZMO_VS_CODE = "\
cbuffer cbPerFrame : register(b0)\
{\
	float4x4 matVP;\
	float4x4 matWorld;\
	float4 fColor;\
};\
\
struct VSInput\
{\
	float3 Position : POSITION;\
	float3 Normal : NORMAL;\
};\
\
struct VSOutput\
{\
	float4 Position : SV_POSITION;\
	float4 Color : COLOR;\
};\
\
static const float3 light1 = float3(1, 1, 1);\
static const float3 light2 = float3(-1, 1, -1);\
static const float lightRatio = 0.8f;\
VSOutput main(VSInput vin)\
{\
	VSOutput vout = (VSOutput)0;\
	\
	vin.Normal = normalize(mul(float4(vin.Normal, 0), matWorld)); \
	vout.Position = mul(float4(vin.Position, 1.0f), matWorld);\
	\
	float3 lightVec1 = normalize(light1);\
	float3 lightVec2 = normalize(light2);\
	\
	float lightFactor = saturate(dot(lightVec1, vin.Normal)) + saturate(dot(lightVec2, vin.Normal));\
	vout.Color = fColor;\
	\
	vout.Color = fColor*(1-lightRatio) + lightFactor*fColor*lightRatio;\
	vout.Color.a = 1.0f;\
	\
	vout.Position = mul(vout.Position, matVP);\
	\
	return vout;\
}\0";

const char* GIZMO_PS_CODE = "\
struct PSInput\
{\
	float4 Position : SV_POSITION;\
	float4 Color : COLOR;\
};\
\
float4 main(PSInput pin) : SV_TARGET\
{\
	return pin.Color;\
}\0";
#define GIZMO_SCALE_FACTOR 7.5f
#define GIZMO_HEIGHT 1.0f
#define GIZMO_WIDTH 0.05f
#define GIZMO_POINTER_WIDTH 0.1f
#define GIZMO_POINTER_HEIGHT 0.175f

namespace ed
{
	GizmoObject::GizmoObject(ml::Window * wnd) :
		m_wnd(wnd),
		m_axisSelected(-1),
		m_mode(0)
	{
		m_vlayout.Add("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0);
		m_vlayout.Add("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 16);
		m_vs.InputSignature = &m_vlayout;

		m_vs.LoadFromMemory(*m_wnd, GIZMO_VS_CODE, strlen(GIZMO_VS_CODE), "main");
		m_ps.LoadFromMemory(*m_wnd, GIZMO_PS_CODE, strlen(GIZMO_PS_CODE), "main");

		m_handle = ml::GeometryFactory::CreateCube(GIZMO_WIDTH, GIZMO_HEIGHT, GIZMO_WIDTH, *m_wnd);

		m_cb.Create(*m_wnd, &m_cbData, sizeof(m_cbData));

		m_xWorld = DirectX::XMMatrixIdentity();
		m_yWorld = DirectX::XMMatrixIdentity();
		m_zWorld = DirectX::XMMatrixIdentity();

		m_buildPointer();
	}
	GizmoObject::~GizmoObject()
	{
	}
	int GizmoObject::Click(int sx, int sy, int vw, int vh)
	{
		m_axisSelected = -1;
		m_vw = vw;
		m_vh = vh;

		DirectX::XMMATRIX proj = SystemVariableManager::Instance().GetProjectionMatrix();
		DirectX::XMFLOAT4X4 proj4x4; DirectX::XMStoreFloat4x4(&proj4x4, proj);

		float vx = (+2.0f*sx / vw - 1.0f) / proj4x4(0, 0);
		float vy = (-2.0f*sy / vh + 1.0f) / proj4x4(1, 1);

		DirectX::XMVECTOR rayOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		DirectX::XMVECTOR rayDir = DirectX::XMVectorSet(vx, vy, 1.0f, 0.0f);

		DirectX::XMMATRIX view = SystemVariableManager::Instance().GetCamera()->GetMatrix();
		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&XMMatrixDeterminant(view), view);

		rayOrigin = DirectX::XMVector3TransformCoord(rayOrigin, invView);
		rayDir = DirectX::XMVector3TransformNormal(rayDir, invView);

		DirectX::BoundingBox handleBox;
		handleBox.Center = DirectX::XMFLOAT3(0, 0, 0);
		handleBox.Extents = DirectX::XMFLOAT3(GIZMO_WIDTH / 2, (GIZMO_HEIGHT + GIZMO_POINTER_HEIGHT) / 2, GIZMO_WIDTH / 2);

		// X axis
		DirectX::XMMATRIX invWorld = DirectX::XMMatrixInverse(&XMMatrixDeterminant(m_xWorld), m_xWorld);

		DirectX::XMVECTOR rayOrigin2 = DirectX::XMVector3TransformCoord(rayOrigin, invWorld);
		DirectX::XMVECTOR rayDir2 = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(rayDir, invWorld));

		float distX, distY, distZ, dist = std::numeric_limits<float>::infinity();;
		if (handleBox.Intersects(rayOrigin2, rayDir2, distX))
			m_axisSelected = 0, dist = distX;
		else distX = std::numeric_limits<float>::infinity();

		// Y axis
		invWorld = DirectX::XMMatrixInverse(&XMMatrixDeterminant(m_yWorld), m_yWorld);
		rayOrigin2 = DirectX::XMVector3TransformCoord(rayOrigin, invWorld);
		rayDir2 = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(rayDir, invWorld));

		if (handleBox.Intersects(rayOrigin2, rayDir2, distY) && distY < distX)
			m_axisSelected = 1, dist = distY;
		else distY = std::numeric_limits<float>::infinity();

		// Z axis
		invWorld = DirectX::XMMatrixInverse(&XMMatrixDeterminant(m_zWorld), m_zWorld);
		rayOrigin2 = DirectX::XMVector3TransformCoord(rayOrigin, invWorld);
		rayDir2 = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(rayDir, invWorld));

		if (handleBox.Intersects(rayOrigin2, rayDir2, distZ) && distZ < distY && distZ < distX)
			m_axisSelected = 2, dist = distZ;
		else distZ = std::numeric_limits<float>::infinity();

		if (m_axisSelected != -1) {
			m_clickDepth = dist;
			m_clickStart = DirectX::XMVectorAdd(rayOrigin, DirectX::XMVectorScale(rayDir, m_clickDepth));
		}

		return m_axisSelected;
	}
	void GizmoObject::Move(int dx, int dy)
	{
		if (m_axisSelected == -1)
			return;

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

		// X axis
			m_cbData.fColor = DirectX::XMFLOAT4(1, 0, 0, 1);

			m_xWorld = DirectX::XMMatrixScaling(scale, scale, scale) *
				DirectX::XMMatrixRotationZ(DirectX::XM_PIDIV2) *
				DirectX::XMMatrixTranslation(m_trans->x + scale * (GIZMO_HEIGHT / 2 + GIZMO_WIDTH / 2), m_trans->y, m_trans->z);
			DirectX::XMStoreFloat4x4(&m_cbData.matWorld, DirectX::XMMatrixTranspose(m_xWorld));
			m_cb.Update(&m_cbData);

			m_handle.Draw();

			DirectX::XMMATRIX prtWorld = DirectX::XMMatrixScaling(scale, scale, scale) *
				DirectX::XMMatrixRotationZ(-DirectX::XM_PIDIV2) *
				DirectX::XMMatrixTranslation(m_trans->x + scale * GIZMO_HEIGHT + scale * GIZMO_WIDTH / 2, m_trans->y, m_trans->z);
			DirectX::XMStoreFloat4x4(&m_cbData.matWorld, DirectX::XMMatrixTranspose(prtWorld));
			m_cb.Update(&m_cbData);

			m_pointer.Bind();
			m_wnd->Draw(m_ptrVerts.size());

		// Y axis
			m_cbData.fColor = DirectX::XMFLOAT4(0, 1, 0, 1);

			m_yWorld = DirectX::XMMatrixScaling(scale, scale, scale) *
				DirectX::XMMatrixTranslation(m_trans->x, m_trans->y+scale*(GIZMO_HEIGHT/2- GIZMO_WIDTH/2), m_trans->z);
			DirectX::XMStoreFloat4x4(&m_cbData.matWorld, DirectX::XMMatrixTranspose(m_yWorld));
			m_cb.Update(&m_cbData);

			m_handle.Draw();

			prtWorld = DirectX::XMMatrixScaling(scale, scale, scale) *
				DirectX::XMMatrixTranslation(m_trans->x, m_trans->y + scale * GIZMO_HEIGHT - scale*GIZMO_WIDTH/2, m_trans->z);
			DirectX::XMStoreFloat4x4(&m_cbData.matWorld, DirectX::XMMatrixTranspose(prtWorld));
			m_cb.Update(&m_cbData);

			m_pointer.Bind();
			m_wnd->Draw(m_ptrVerts.size());

		// Z axis
			m_cbData.fColor = DirectX::XMFLOAT4(0, 0, 1, 1);

			m_zWorld = DirectX::XMMatrixScaling(scale, scale, scale) *
				DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2) *
				DirectX::XMMatrixTranslation(m_trans->x, m_trans->y, m_trans->z + scale * (GIZMO_HEIGHT / 2 + GIZMO_WIDTH / 2));
			DirectX::XMStoreFloat4x4(&m_cbData.matWorld, DirectX::XMMatrixTranspose(m_zWorld));
			m_cb.Update(&m_cbData);

			m_handle.Draw();

			prtWorld = DirectX::XMMatrixScaling(scale, scale, scale) *
				DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2) *
				DirectX::XMMatrixTranslation(m_trans->x, m_trans->y, m_trans->z + scale * GIZMO_HEIGHT + scale * GIZMO_WIDTH / 2);
			DirectX::XMStoreFloat4x4(&m_cbData.matWorld, DirectX::XMMatrixTranspose(prtWorld));
			m_cb.Update(&m_cbData);

			m_pointer.Bind();
			m_wnd->Draw(m_ptrVerts.size());
	}
	void GizmoObject::m_buildPointer()
	{

		m_ptrVerts.resize(18);

		m_ptrVerts[0].Position = DirectX::XMFLOAT4(-GIZMO_POINTER_WIDTH / 2, 0, -GIZMO_POINTER_WIDTH / 2, 1);
		m_ptrVerts[1].Position = DirectX::XMFLOAT4(GIZMO_POINTER_WIDTH / 2, 0, -GIZMO_POINTER_WIDTH / 2, 1);
		m_ptrVerts[2].Position = DirectX::XMFLOAT4(GIZMO_POINTER_WIDTH / 2, 0, GIZMO_POINTER_WIDTH / 2, 1);


		m_ptrVerts[3].Position = DirectX::XMFLOAT4(GIZMO_POINTER_WIDTH / 2, 0, GIZMO_POINTER_WIDTH / 2, 1);
		m_ptrVerts[4].Position = DirectX::XMFLOAT4(-GIZMO_POINTER_WIDTH / 2, 0, GIZMO_POINTER_WIDTH / 2, 1);
		m_ptrVerts[5].Position = DirectX::XMFLOAT4(-GIZMO_POINTER_WIDTH / 2, 0, -GIZMO_POINTER_WIDTH / 2, 1);
		m_ptrVerts[0].Normal = m_ptrVerts[1].Normal = m_ptrVerts[2].Normal =
			m_ptrVerts[3].Normal = m_ptrVerts[4].Normal = m_ptrVerts[5].Normal = DirectX::XMFLOAT4(0, 1, 0, 0);

		m_ptrVerts[6].Position = DirectX::XMFLOAT4(GIZMO_POINTER_WIDTH / 2, 0, GIZMO_POINTER_WIDTH / 2, 1);
		m_ptrVerts[7].Position = DirectX::XMFLOAT4(GIZMO_POINTER_WIDTH / 2, 0, -GIZMO_POINTER_WIDTH / 2, 1);
		m_ptrVerts[8].Position = DirectX::XMFLOAT4(0, GIZMO_POINTER_HEIGHT, 0, 1);
		DirectX::XMVECTOR p1 = DirectX::XMVectorSubtract(DirectX::XMLoadFloat4(&m_ptrVerts[6].Position), DirectX::XMLoadFloat4(&m_ptrVerts[7].Position));
		DirectX::XMVECTOR p2 = DirectX::XMVectorSubtract(DirectX::XMLoadFloat4(&m_ptrVerts[7].Position), DirectX::XMLoadFloat4(&m_ptrVerts[8].Position));
		DirectX::XMStoreFloat4(&m_ptrVerts[6].Normal, DirectX::XMVector3Cross(p1, p2));
		m_ptrVerts[7].Normal = m_ptrVerts[8].Normal = m_ptrVerts[6].Normal;

		m_ptrVerts[9].Position = DirectX::XMFLOAT4(0, GIZMO_POINTER_HEIGHT, 0, 1);
		m_ptrVerts[10].Position = DirectX::XMFLOAT4(-GIZMO_POINTER_WIDTH / 2, 0, GIZMO_POINTER_WIDTH / 2, 1);
		m_ptrVerts[11].Position = DirectX::XMFLOAT4(GIZMO_POINTER_WIDTH / 2, 0, GIZMO_POINTER_WIDTH / 2, 1);
		p1 = DirectX::XMVectorSubtract(DirectX::XMLoadFloat4(&m_ptrVerts[9].Position), DirectX::XMLoadFloat4(&m_ptrVerts[10].Position));
		p2 = DirectX::XMVectorSubtract(DirectX::XMLoadFloat4(&m_ptrVerts[10].Position), DirectX::XMLoadFloat4(&m_ptrVerts[11].Position));
		DirectX::XMStoreFloat4(&m_ptrVerts[9].Normal, DirectX::XMVector3Cross(p1, p2));
		m_ptrVerts[10].Normal = m_ptrVerts[11].Normal = m_ptrVerts[9].Normal;

		m_ptrVerts[12].Position = DirectX::XMFLOAT4(0, GIZMO_POINTER_HEIGHT, 0, 1);
		m_ptrVerts[13].Position = DirectX::XMFLOAT4(-GIZMO_POINTER_WIDTH / 2, 0, -GIZMO_POINTER_WIDTH / 2, 1);
		m_ptrVerts[14].Position = DirectX::XMFLOAT4(-GIZMO_POINTER_WIDTH / 2, 0, GIZMO_POINTER_WIDTH / 2, 1);
		p1 = DirectX::XMVectorSubtract(DirectX::XMLoadFloat4(&m_ptrVerts[12].Position), DirectX::XMLoadFloat4(&m_ptrVerts[13].Position));
		p2 = DirectX::XMVectorSubtract(DirectX::XMLoadFloat4(&m_ptrVerts[13].Position), DirectX::XMLoadFloat4(&m_ptrVerts[14].Position));
		DirectX::XMStoreFloat4(&m_ptrVerts[12].Normal, DirectX::XMVector3Cross(p1, p2));
		m_ptrVerts[13].Normal = m_ptrVerts[14].Normal = m_ptrVerts[12].Normal;

		m_ptrVerts[15].Position = DirectX::XMFLOAT4(-GIZMO_POINTER_WIDTH / 2, 0, -GIZMO_POINTER_WIDTH / 2, 1);
		m_ptrVerts[16].Position = DirectX::XMFLOAT4(0, GIZMO_POINTER_HEIGHT, 0, 1);
		m_ptrVerts[17].Position = DirectX::XMFLOAT4(GIZMO_POINTER_WIDTH / 2, 0, -GIZMO_POINTER_WIDTH / 2, 1);
		p1 = DirectX::XMVectorSubtract(DirectX::XMLoadFloat4(&m_ptrVerts[15].Position), DirectX::XMLoadFloat4(&m_ptrVerts[16].Position));
		p2 = DirectX::XMVectorSubtract(DirectX::XMLoadFloat4(&m_ptrVerts[16].Position), DirectX::XMLoadFloat4(&m_ptrVerts[17].Position));
		DirectX::XMStoreFloat4(&m_ptrVerts[15].Normal, DirectX::XMVector3Cross(p1, p2));
		m_ptrVerts[16].Normal = m_ptrVerts[17].Normal = m_ptrVerts[15].Normal;

		m_pointer.Create(*m_wnd, m_ptrVerts.data(), m_ptrVerts.size());
	}
}