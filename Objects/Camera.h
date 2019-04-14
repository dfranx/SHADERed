#pragma once
#include <DirectXMath.h>

namespace ed
{
	class Camera
	{
	public:
		virtual void Reset() {};

		virtual DirectX::XMFLOAT3 GetRotation() = 0;

		virtual DirectX::XMVECTOR GetPosition() = 0;
		virtual DirectX::XMVECTOR GetUpVector() = 0;

		virtual DirectX::XMMATRIX GetMatrix() = 0;
	};
}