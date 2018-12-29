#pragma once
#include <string>
#include <Windows.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <MoonLight/Base/Color.h>

namespace ed
{
	class UIHelper
	{
	public:
		static std::string GetOpenFileDialog(HWND wnd, LPCWSTR files = L"All\0*.*\0HLSL\0*.hlsl;.hlsli\0");
		static void CreateInputFloat3(const char* name, DirectX::XMFLOAT3& data);
		static bool CreateInputColor(const char* name, ml::Color& data);
		static bool DisplayBlendOperatorCombo(const char* name, D3D11_BLEND_OP& op);
		static bool DisplayBlendCombo(const char* name, D3D11_BLEND& blend);
	};
}