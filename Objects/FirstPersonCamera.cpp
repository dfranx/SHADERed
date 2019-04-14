#include "FirstPersonCamera.h"
#include <algorithm>

using namespace DirectX;

#define FORWARD_VECTOR DirectX::XMVectorSet(0, 0, 1, 0)
#define RIGHT_VECTOR DirectX::XMVectorSet(1, 0, 0, 0)
#define UP_VECTOR DirectX::XMVectorSet(0, 1, 0, 0)

namespace ed
{
	void FirstPersonCamera::Reset()
	{
		m_pos = DirectX::XMFLOAT3(0.0f, 0.0f, 7.0f);
	}
	void FirstPersonCamera::MoveLeftRight(float d)
	{
		DirectX::XMMATRIX rotaMatrix;
		rotaMatrix = XMMatrixRotationY(m_yaw);
		
		DirectX::XMVECTOR right = XMVector3TransformCoord(RIGHT_VECTOR, rotaMatrix);

		auto pos = DirectX::XMLoadFloat3(&m_pos);

		pos += right * d;
		DirectX::XMStoreFloat3(&m_pos, pos);
	}
	void FirstPersonCamera::MoveUpDown(float d)
	{
		DirectX::XMMATRIX rotaMatrix;
		rotaMatrix = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0);

		DirectX::XMVECTOR forward = XMVector3TransformCoord(FORWARD_VECTOR, rotaMatrix);
		auto pos = DirectX::XMLoadFloat3(&m_pos);

		pos += forward * d;
		DirectX::XMStoreFloat3(&m_pos, pos);
	}
	void FirstPersonCamera::Yaw(float yaw)
	{
		m_yaw += yaw;
	}
	void FirstPersonCamera::Pitch(float p)
	{
		m_pitch += p;
	}
	DirectX::XMVECTOR FirstPersonCamera::GetUpVector()
	{
		XMMATRIX yawMatrix;
		yawMatrix = XMMatrixRotationY(m_yaw);

		return XMVector3TransformCoord(DirectX::XMVectorSet(0,1,0,1), yawMatrix);
	}
	XMMATRIX FirstPersonCamera::GetMatrix()
	{
		DirectX::XMMATRIX rota = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0);

		DirectX::XMVECTOR target = XMVector3TransformCoord(FORWARD_VECTOR, rota);
		target = DirectX::XMLoadFloat3(&m_pos) - XMVector3Normalize(target);

		DirectX::XMMATRIX yawMatrix;
		yawMatrix = XMMatrixRotationY(m_yaw);

		DirectX::XMVECTOR right = XMVector3TransformCoord(RIGHT_VECTOR, yawMatrix);
		DirectX::XMVECTOR up = XMVector3TransformCoord(UP_VECTOR, yawMatrix);
		DirectX::XMVECTOR forward = XMVector3TransformCoord(FORWARD_VECTOR, yawMatrix);

		return XMMatrixLookAtLH(DirectX::XMLoadFloat3(&m_pos), target, up);
	}
}
