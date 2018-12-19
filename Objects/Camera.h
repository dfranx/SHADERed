#pragma once
#include <DirectXMath.h>

namespace ed
{
	class Camera
	{
	public:
		Camera();

		void Reset();

		void SetDistance(float d);
		void Move(float d);
		void RotateX(float rx);
		void RotateY(float ry);
		void RotateZ(float rz);

		inline float GetDistance() { return distance; }
		inline DirectX::XMFLOAT3 GetRotation() { return DirectX::XMFLOAT3(rotaX, rotaY, rotaZ); }

		DirectX::XMVECTOR GetPosition();
		DirectX::XMVECTOR GetUpVector();

		DirectX::XMMATRIX GetMatrix();

	private:
		const float maxDistance = 50, minDistance = 2;
		const float maxRotaY = 89;
		float distance;
		float rotaY, rotaX, rotaZ;
	};
}