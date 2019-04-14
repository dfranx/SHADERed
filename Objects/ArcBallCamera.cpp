#include "ArcBallCamera.h"
#include <algorithm>

using namespace DirectX;

namespace ed
{
	void ArcBallCamera::Reset()
	{
		distance = 7;
		rotaX = 0;
		rotaY = 0;
		rotaZ = 0;
	}
	void ArcBallCamera::SetDistance(float d)
	{
		distance = std::max(minDistance, std::min(maxDistance, d));
	}
	void ArcBallCamera::Move(float d)
	{
		distance = std::max(minDistance, std::min(maxDistance, distance + d));
	}
	void ArcBallCamera::RotateX(float rx)
	{
		rotaX += rx;
		if (rotaX >= 360)
			rotaX -= 360;
		if (rotaX <= 0)
			rotaX += 360;
	}
	void ArcBallCamera::RotateY(float ry)
	{
		rotaY = std::max(-maxRotaY, std::min(maxRotaY, rotaY + ry));
	}
	void ArcBallCamera::RotateZ(float rz)
	{
		rotaZ += rz;
		if (rotaZ >= 360)
			rotaZ -= 360;
		if (rotaZ <= 0)
			rotaZ += 360;
	}
	DirectX::XMVECTOR ArcBallCamera::GetPosition()
	{
		XMVECTOR pos = XMVectorSet(0, 0, -distance, 0);
		XMMATRIX rotaMat = XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotaY), XMConvertToRadians(rotaX), XMConvertToRadians(rotaZ));

		pos = XMVector3Transform(pos, rotaMat);

		return pos;
	}
	DirectX::XMVECTOR ArcBallCamera::GetUpVector()
	{
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);
		XMMATRIX rotaMat = XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotaY), XMConvertToRadians(rotaX), XMConvertToRadians(rotaZ));

		up = XMVector3Transform(up, rotaMat);

		return XMVector3Normalize(up);
	}
	XMMATRIX ArcBallCamera::GetMatrix()
	{
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);
		XMVECTOR pos = XMVectorSet(0, 0, -distance, 0);
		XMMATRIX rotaMat = XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotaY), XMConvertToRadians(rotaX), XMConvertToRadians(rotaZ));

		up = XMVector3Transform(up, rotaMat);
		pos = XMVector3Transform(pos, rotaMat);

		XMMATRIX ret = XMMatrixLookAtLH(pos, XMVectorSet(0, 0, 0, 0), up);

		return ret;
	}
}
