#pragma once
#include <DirectXMath.h>
#include "Camera.h"

namespace ed
{
	class FirstPersonCamera : public Camera
	{
	public:
		FirstPersonCamera() { Reset(); }

		virtual void Reset();
		
		inline void SetPosition(float x, float y, float z) { m_pos = DirectX::XMFLOAT3(x, y, z); }

		void MoveLeftRight(float d);
		void MoveUpDown(float d);
		inline void Yaw(float y) { m_yaw += y; }
		inline void Pitch(float p) { m_pitch += p; }
		inline void SetYaw(float y) { m_yaw = y; }
		inline void SetPitch(float p) { m_pitch = p; }

		virtual inline DirectX::XMFLOAT3 GetRotation() { return DirectX::XMFLOAT3(m_yaw, m_pitch, 0); }

		virtual inline DirectX::XMVECTOR GetPosition() { return DirectX::XMLoadFloat3(&m_pos); }
		virtual DirectX::XMVECTOR GetUpVector();

		virtual DirectX::XMMATRIX GetMatrix();

	private:
		DirectX::XMFLOAT3 m_pos;

		float m_yaw = 0.0f;
		float m_pitch = 0.0f;
	};
}