#include "UIHelper.h"
#include "../ImGUI/imgui.h"
#include "../Objects/Names.h"

namespace ed
{
	std::string UIHelper::GetOpenFileDialog(HWND wnd, LPCWSTR files)
	{
		OPENFILENAME dialog;
		TCHAR filePath[MAX_PATH] = { 0 };

		ZeroMemory(&dialog, sizeof(dialog));
		dialog.lStructSize = sizeof(dialog);
		dialog.hwndOwner = wnd;
		dialog.lpstrFile = filePath;
		dialog.nMaxFile = sizeof(filePath);
		dialog.lpstrFilter = files;
		dialog.nFilterIndex = 1;
		dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileName(&dialog) == TRUE) {
			// TODO: get relative path to project file

			std::wstring wfile = std::wstring(filePath);
			return std::string(wfile.begin(), wfile.end());
		}
		return "";
	}
	void UIHelper::CreateInputFloat3(const char * name, DirectX::XMFLOAT3& data)
	{
		float val[3] = { data.x , data.y, data.z };
		ImGui::DragFloat3(name, val, 0.01f);
		data = DirectX::XMFLOAT3(val);
	}
	bool UIHelper::CreateInputFloat2(const char * name, DirectX::XMFLOAT2& data)
	{
		float val[2] = { data.x , data.y };
		bool ret = ImGui::DragFloat2(name, val, 0.01f);
		data = DirectX::XMFLOAT2(val);

		return ret;
	}
	bool UIHelper::CreateInputInt2(const char * name, DirectX::XMINT2 & data)
	{
		int val[2] = { data.x , data.y };
		bool ret = ImGui::DragInt2(name, val);
		data = DirectX::XMINT2(val);

		return ret;
	}
	bool UIHelper::CreateInputColor(const char * name, ml::Color & data)
	{
		bool ret = false;

		float val[4] = { data.R / 255.0f , data.G / 255.0f, data.B / 255.0f, data.A / 255.0f };
		if (ImGui::ColorEdit4(name, val, ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_AlphaPreview))
			ret = true;
		data.R = val[0] * 255;
		data.G = val[1] * 255;
		data.B = val[2] * 255;
		data.A = val[3] * 255;

		return ret;
	}
	bool UIHelper::CreateBlendOperatorCombo(const char* name, D3D11_BLEND_OP& op)
	{
		bool ret = false;

		if (ImGui::BeginCombo(name, BLEND_OPERATOR_NAMES[op])) {
			for (int i = 0; i < _ARRAYSIZE(BLEND_OPERATOR_NAMES); i++) {
				bool is_selected = ((int)op == i);

				if (strlen(BLEND_OPERATOR_NAMES[i]) > 1)
					if (ImGui::Selectable(BLEND_OPERATOR_NAMES[i], is_selected)) {
						op = (D3D11_BLEND_OP)i;
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
	bool UIHelper::CreateBlendCombo(const char* name, D3D11_BLEND& blend)
	{
		bool ret = false;

		if (ImGui::BeginCombo(name, BLEND_NAMES[blend])) {
			for (int i = 0; i < _ARRAYSIZE(BLEND_NAMES); i++) {
				bool is_selected = ((int)blend == i);

				if (strlen(BLEND_NAMES[i]) > 1)
					if (ImGui::Selectable(BLEND_NAMES[i], is_selected)) {
						blend = (D3D11_BLEND)i;
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
	bool UIHelper::CreateCullModeCombo(const char * name, D3D11_CULL_MODE& cull)
	{
		bool ret = false;

		if (ImGui::BeginCombo(name, CULL_MODE_NAMES[cull])) {
			for (int i = 0; i < _ARRAYSIZE(CULL_MODE_NAMES); i++) {
				bool is_selected = ((int)cull == i);

				if (strlen(CULL_MODE_NAMES[i]) > 1)
					if (ImGui::Selectable(CULL_MODE_NAMES[i], is_selected)) {
						cull = (D3D11_CULL_MODE)i;
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
	bool UIHelper::CreateComparisonFunctionCombo(const char * name, D3D11_COMPARISON_FUNC & comp)
	{
		bool ret = false;

		if (ImGui::BeginCombo(name, COMPARISON_FUNCTION_NAMES[comp])) {
			for (int i = 0; i < _ARRAYSIZE(COMPARISON_FUNCTION_NAMES); i++) {
				bool is_selected = ((int)comp == i);

				if (strlen(COMPARISON_FUNCTION_NAMES[i]) > 1)
					if (ImGui::Selectable(COMPARISON_FUNCTION_NAMES[i], is_selected)) {
						comp = (D3D11_COMPARISON_FUNC)i;
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
	bool UIHelper::CreateStencilOperationCombo(const char * name, D3D11_STENCIL_OP & op)
	{
		bool ret = false;

		if (ImGui::BeginCombo(name, STENCIL_OPERATION_NAMES[op])) {
			for (int i = 0; i < _ARRAYSIZE(STENCIL_OPERATION_NAMES); i++) {
				bool is_selected = ((int)op == i);

				if (strlen(STENCIL_OPERATION_NAMES[i]) > 1)
					if (ImGui::Selectable(STENCIL_OPERATION_NAMES[i], is_selected)) {
						op = (D3D11_STENCIL_OP)i;
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
}