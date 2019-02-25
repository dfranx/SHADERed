#include "GizmoObject.h"
#include "SystemVariableManager.h"
#include <MoonLight/Base/GeometryFactory.h>

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
static const float3 dirLight = float3(2.5f, -2.5f, 10);\
static const float lightRatio = 0.7f;\
VSOutput main(VSInput vin)\
{\
	VSOutput vout = (VSOutput)0;\
	\
	float3 tNormal = vin.Normal;\
	float3 lightVec = -dirLight;\
	float lightFactor = saturate(dot(normalize(lightVec), normalize(tNormal))) ;\
	\
	vout.Position = mul(mul(float4(vin.Position, 1.0f), matWorld), matVP);\
	vout.Color = fColor*(1-lightFactor) + lightFactor*fColor*lightRatio;\
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
const float GIZMO_SCALE_FACTOR = 7.5f;

namespace ed
{
	GizmoObject::GizmoObject(ml::Window * wnd) :
		m_wnd(wnd),
		m_axisSelected(-1),
		m_mode(0)
	{
		m_vlayout.Add("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0);
		m_vlayout.Add("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0);
		m_vs.InputSignature = &m_vlayout;

		m_vs.LoadFromMemory(*m_wnd, GIZMO_VS_CODE, strlen(GIZMO_VS_CODE), "main");
		m_ps.LoadFromMemory(*m_wnd, GIZMO_PS_CODE, strlen(GIZMO_PS_CODE), "main");

		m_testCube = ml::GeometryFactory::CreateCube(0.5f, 0.5f, 0.5f, *m_wnd);

		m_cb.Create(*m_wnd, &m_cbData, sizeof(m_cbData));
	}
	GizmoObject::~GizmoObject()
	{
	}
	int GizmoObject::Click(int sx, int sy)
	{
		m_axisSelected = -1;
		return m_axisSelected;
	}
	void GizmoObject::Move(int dx, int dy)
	{
		if (m_axisSelected == -1)
			return;
	}
	void GizmoObject::Render()
	{
		float scale = DirectX::XMVectorGetX(
						DirectX::XMVector3Length(
							DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(m_trans), SystemVariableManager::Instance().GetCamera().GetPosition()
					  ))) / GIZMO_SCALE_FACTOR;

		DirectX::XMMATRIX world = DirectX::XMMatrixScaling(scale, scale, scale) *
			DirectX::XMMatrixTranslation(m_trans->x, m_trans->y, m_trans->z);

		DirectX::XMStoreFloat4x4(&m_cbData.matWorld, DirectX::XMMatrixTranspose(world));

		m_cbData.fColor = DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
		m_cb.Update(&m_cbData);

		m_vs.Bind();
		m_ps.Bind();
		m_wnd->SetInputLayout(m_vlayout);
		m_wnd->SetTopology(ml::Topology::TriangleList);
		m_cb.BindVS(0);
		
		m_testCube.Draw();
	}
}