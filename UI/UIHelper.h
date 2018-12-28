#pragma once
#include <string>
#include <Windows.h>
#include <DirectXMath.h>

namespace ed
{
	class UIHelper
	{
	public:
		static std::string GetOpenFileDialog(HWND wnd, LPCWSTR files = L"All\0*.*\0HLSL\0*.hlsl;.hlsli\0");
		static void CreateInputFloat3(const char* name, DirectX::XMFLOAT3& data);
	};
}