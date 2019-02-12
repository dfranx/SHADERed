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
		static bool CreateInputFloat2(const char* name, DirectX::XMFLOAT2& data);
		static bool CreateInputInt2(const char* name, DirectX::XMINT2& data);
		static bool CreateInputColor(const char* name, ml::Color& data);
		static bool CreateBlendOperatorCombo(const char* name, D3D11_BLEND_OP& op);
		static bool CreateBlendCombo(const char* name, D3D11_BLEND& blend);
		static bool CreateCullModeCombo(const char* name, D3D11_CULL_MODE& cull);
		static bool CreateComparisonFunctionCombo(const char* name, D3D11_COMPARISON_FUNC& comp);
		static bool CreateStencilOperationCombo(const char* name, D3D11_STENCIL_OP& op);
	};
}