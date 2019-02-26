#pragma once
#include <MoonLight/Base/Geometry.h>
#include <MoonLight/Base/PixelShader.h>
#include <MoonLight/Base/VertexShader.h>
#include <MoonLight/Base/ConstantBuffer.h>

namespace ed
{
	class GizmoObject
	{
	public:
		GizmoObject(ml::Window* wnd);
		~GizmoObject();

		inline void SetProjectionMatrix(DirectX::XMMATRIX proj) { DirectX::XMStoreFloat4x4(&m_proj, proj); }
		inline void SetViewMatrix(DirectX::XMMATRIX view) { DirectX::XMStoreFloat4x4(&m_cbData.matVP, DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(view, DirectX::XMLoadFloat4x4(&m_proj)))); }
		inline void SetTransform(DirectX::XMFLOAT3* t, DirectX::XMFLOAT3* s, DirectX::XMFLOAT3* r) { m_trans = t; m_scale = s; m_rota = r; }

		int Click(int sx, int sy, int vw, int wh);
		void Move(int dx, int dy);

		void Render();

	private:
		ml::Window* m_wnd;
		ml::PixelShader m_ps;
		ml::VertexShader m_vs;
		ml::VertexInputLayout m_vlayout;
		DirectX::XMFLOAT4X4 m_proj;
		DirectX::XMMATRIX m_xWorld, m_yWorld, m_zWorld;
		DirectX::XMFLOAT3 *m_trans, *m_scale, *m_rota;
		int m_axisSelected; // -1 = none, 0 = x, 1 = y, 2 = z
		int m_mode; // 0 = translation, 1 = scale, 2 = rotation

		ml::Geometry m_handle;

		std::vector<ml::Geometry::Vertex> m_ptrVerts;
		ml::VertexBuffer<ml::Geometry::Vertex> m_pointer;
		void m_buildPointer();

		struct CBGizmo
		{
			DirectX::XMFLOAT4X4 matVP;
			DirectX::XMFLOAT4X4 matWorld;
			DirectX::XMFLOAT4 fColor;
		} m_cbData;
		ml::ConstantBuffer<CBGizmo> m_cb;
	};
}