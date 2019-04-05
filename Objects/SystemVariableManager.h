#pragma once
#include <MoonLight/Base/Timer.h>
#include <DirectXMath.h>
#include "ShaderVariable.h"
#include "PipelineItem.h"
#include "Camera.h"

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
			}

			return ed::ShaderVariable::ValueType::Float1;
		}

		void Update(ed::ShaderVariable* var);

		inline Camera& GetCamera() { return m_cam; }
		inline DirectX::XMMATRIX GetViewMatrix() { return m_cam.GetMatrix(); }
		inline DirectX::XMMATRIX GetProjectionMatrix() { return DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45), (float)m_viewport.x / m_viewport.y, 0.1f, 1000.0f); }
		inline DirectX::XMMATRIX GetOrthographicMatrix() { return DirectX::XMMatrixOrthographicLH(m_viewport.x, m_viewport.y, 0.1f, 1000.0f); }
		inline DirectX::XMMATRIX GetViewProjectionMatrix() { return GetViewMatrix() * GetProjectionMatrix(); }
		inline DirectX::XMMATRIX GetViewOrthographicMatrix() { return GetViewMatrix() * GetOrthographicMatrix(); }
		inline DirectX::XMMATRIX GetGeometryTransform() { return m_geometryTransform; }
		inline DirectX::XMFLOAT2 GetViewportSize() { return m_viewport; }
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

	private:
		ml::Timer m_timer;
		float m_deltaTime;
		Camera m_cam;
		DirectX::XMFLOAT2 m_viewport, m_mouse;
		DirectX::XMMATRIX m_geometryTransform;
		bool m_isPicked;
	};
}