#pragma once
#include <DirectXMath.h>

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

		inline DirectX::XMMATRIX GetViewMatrix() { return DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(45)) * DirectX::XMMatrixTranslation(0, 0, 10); }
		inline DirectX::XMMATRIX GetProjectionMatrix() { return DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45), (float)m_viewport.x / m_viewport.y, 0.1f, 1000.0f); }
		inline DirectX::XMMATRIX GetViewProjectionMatrix() { return GetViewMatrix() * GetProjectionMatrix(); }
		inline DirectX::XMFLOAT2 GetViewportSize() { return m_viewport; }

		inline void SetViewportSize(float x, float y) { m_viewport = DirectX::XMFLOAT2(x, y); }

	private:
		DirectX::XMFLOAT2 m_viewport;
	};
}