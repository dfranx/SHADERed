#pragma once
#include <DirectXMath.h>
#include "Camera.h"

namespace ed
{
	class ArcBallCamera : public Camera
	{
	public:
		ArcBallCamera() { Reset(); }

		virtual void Reset();

		void SetDistance(float d);
		void Move(float d);
		void RotateX(float rx);
		void RotateY(float ry);
		void RotateZ(float rz);

		inline float GetDistance() { return distance; }
		inline virtual DirectX::XMFLOAT3 GetRotation() { return DirectX::XMFLOAT3(rotaX, rotaY, rotaZ); }

		virtual DirectX::XMVECTOR GetPosition();
		virtual DirectX::XMVECTOR GetUpVector();

		virtual DirectX::XMMATRIX GetMatrix();

	private:
		const float maxDistance = 50, minDistance = 2;
		const float maxRotaY = 89;
		float distance;
		float rotaY, rotaX, rotaZ;
	};
}