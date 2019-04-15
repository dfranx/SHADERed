#pragma once
#include <MoonLight/Base/Timer.h>
#include <DirectXMath.h>
#include "ShaderVariable.h"
#include "PipelineItem.h"
#include "ArcBallCamera.h"
#include "FirstPersonCamera.h"
#include "Settings.h"

namespace ed
{
	// singleton used for getting some system-level values
	class SystemVariableManager
	{
	public:
		static inline SystemVariableManager& Instance()
		{
			static SystemVariableManager ret;
			return ret;
		}

		static inline ed::ShaderVariable::ValueType GetType(ed::SystemShaderVariable sysVar)
		{
			switch (sysVar) {
				case ed::SystemShaderVariable::MousePosition: return ed::ShaderVariable::ValueType::Float2;
				case ed::SystemShaderVariable::Projection: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::Time: return ed::ShaderVariable::ValueType::Float1;
				case ed::SystemShaderVariable::TimeDelta: return ed::ShaderVariable::ValueType::Float1;
				case ed::SystemShaderVariable::View: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::ViewportSize: return ed::ShaderVariable::ValueType::Float2;
				case ed::SystemShaderVariable::ViewProjection: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::Orthographic: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::ViewOrthographic: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::GeometryTransform: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::IsPicked: return ed::ShaderVariable::ValueType::Boolean1;
				case ed::SystemShaderVariable::CameraPosition: return ed::ShaderVariable::ValueType::Float4;
				case ed::SystemShaderVariable::KeysWASD: return ed::ShaderVariable::ValueType::Integer4;
			}

			return ed::ShaderVariable::ValueType::Float1;
		}

		void Update(ed::ShaderVariable* var);

		inline Camera* GetCamera() { return Settings::Instance().Project.FPCamera ? (Camera*)&m_fpCam : (Camera*)&m_abCam; }
		inline DirectX::XMMATRIX GetViewMatrix() { return Settings::Instance().Project.FPCamera ? m_fpCam.GetMatrix() : m_abCam.GetMatrix(); }
		inline DirectX::XMMATRIX GetProjectionMatrix() { return DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45), (float)m_viewport.x / m_viewport.y, 0.1f, 1000.0f); }
		inline DirectX::XMMATRIX GetOrthographicMatrix() { return DirectX::XMMatrixOrthographicLH(m_viewport.x, m_viewport.y, 0.1f, 1000.0f); }
		inline DirectX::XMMATRIX GetViewProjectionMatrix() { return GetViewMatrix() * GetProjectionMatrix(); }
		inline DirectX::XMMATRIX GetViewOrthographicMatrix() { return GetViewMatrix() * GetOrthographicMatrix(); }
		inline DirectX::XMMATRIX GetGeometryTransform() { return m_geometryTransform; }
		inline DirectX::XMFLOAT2 GetViewportSize() { return m_viewport; }
		inline DirectX::XMINT4 GetKeysWASD() { return m_wasd; }
		inline DirectX::XMFLOAT2 GetMousePosition() { return m_mouse; }
		inline float GetTime() { return m_timer.GetElapsedTime(); }
		inline float GetTimeDelta() { return m_deltaTime; }
		inline bool IsPicked() { return m_isPicked; }

		inline void SetGeometryTransform(const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT3& rota, const DirectX::XMFLOAT3& pos)
		{
			m_geometryTransform = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z) *
				DirectX::XMMatrixRotationRollPitchYaw(rota.x, rota.y, rota.z) *
				DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		}
		inline void SetViewportSize(float x, float y) { m_viewport = DirectX::XMFLOAT2(x, y); }
		inline void SetMousePosition(float x, float y) { m_mouse = DirectX::XMFLOAT2(x, y); }
		inline void SetTimeDelta(float x) { m_deltaTime = x; }
		inline void SetPicked(bool picked) { m_isPicked = picked; }
		inline void SetKeysWASD(int w, int a, int s, int d) { m_wasd = DirectX::XMINT4(w, a, s, d); }

	private:
		ml::Timer m_timer;
		float m_deltaTime;
		ArcBallCamera m_abCam;
		FirstPersonCamera m_fpCam;
		DirectX::XMFLOAT2 m_viewport, m_mouse;
		DirectX::XMMATRIX m_geometryTransform;
		bool m_isPicked;

		DirectX::XMFLOAT3 m_camPos;
		DirectX::XMINT4 m_wasd;
	};
}