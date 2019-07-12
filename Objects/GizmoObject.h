#pragma once
#include <MoonLight/Base/Geometry.h>
#include <MoonLight/Base/BlendState.h>
#include <MoonLight/Base/PixelShader.h>
#include <MoonLight/Base/VertexShader.h>
#include <MoonLight/Base/ConstantBuffer.h>
#include <MoonLight/Base/RasterizerState.h>
#include <MoonLight/Base/DepthStencilState.h>
#include <MoonLight/Model/OBJModel.h>

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

		inline void SetMode(int mode) { m_mode = mode; m_buildHandles(); }

		inline void UnselectAxis() { m_axisSelected = -1; }
		void HandleMouseMove(int x, int y, int vw, int vh);
		int Click(int sx, int sy, int vw, int wh);
		void Move(int dx, int dy);

		void Render();

	private:
		struct Vertex
		{
			DirectX::XMFLOAT3 Position;
			DirectX::XMFLOAT3 Normal;
			DirectX::XMFLOAT3 Color;
		};

		ml::Window* m_wnd;
		ml::PixelShader m_ps;
		ml::VertexShader m_vs;
		ml::VertexInputLayout m_vlayout;
		DirectX::XMFLOAT4X4 m_proj;
		float m_clickDepth, m_hoverDepth, m_clickDegrees, m_vw, m_vh;
		DirectX::XMVECTOR m_clickStart, m_hoverStart;
		DirectX::XMFLOAT3* m_trans, * m_scale, * m_rota;
		DirectX::XMFLOAT3 m_tValue, m_curValue;
		int m_axisSelected; // -1 = none, 0 = x, 1 = y, 2 = z
		int m_axisHovered;  // ^ same as here
		int m_mode; // 0 = translation, 1 = scale, 2 = rotation

		ml::Geometry m_degreeInfoUI;
		ml::VertexInputLayout m_uiInput;
		ml::PixelShader m_uiPS;
		ml::VertexShader m_uiVS;
		ml::DepthStencilState m_ignoreDepth;
		ml::BlendState m_transparencyBlend;
		ml::RasterizerState m_rasterState;

		DirectX::XMFLOAT3 m_colors[3];
		ml::OBJModel m_model;
		std::vector<Vertex> m_verts;
		bool m_vertsCreated;
		ml::VertexBuffer<Vertex> m_buffer;
		void m_buildHandles();

		__declspec(align(16))
			struct CBDegreeUI
		{
			DirectX::XMFLOAT4X4 matWVP;
		} m_cbUIData;
		ml::ConstantBuffer<CBDegreeUI> m_cbUI;

		__declspec(align(16))
		struct CBGizmo
		{
			DirectX::XMFLOAT4X4 matVP;
			DirectX::XMFLOAT4X4 matWorld;
		} m_cbData;
		ml::ConstantBuffer<CBGizmo> m_cb;
	};
}