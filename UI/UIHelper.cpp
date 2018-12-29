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
	bool UIHelper::DisplayBlendOperatorCombo(const char* name, D3D11_BLEND_OP& op)
	{
		bool ret = false;

		if (ImGui::BeginCombo(name, BLEND_OPERATOR[op])) {
			for (int i = 0; i < _ARRAYSIZE(BLEND_OPERATOR); i++) {
				bool is_selected = ((int)op == i);

				if (strlen(BLEND_OPERATOR[i]) > 1)
					if (ImGui::Selectable(BLEND_OPERATOR[i], is_selected)) {
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
	bool UIHelper::DisplayBlendCombo(const char* name, D3D11_BLEND& blend)
	{
		bool ret = false;

		if (ImGui::BeginCombo(name, BLEND_NAME[blend])) {
			for (int i = 0; i < _ARRAYSIZE(BLEND_NAME); i++) {
				bool is_selected = ((int)blend == i);

				if (strlen(BLEND_NAME[i]) > 1)
					if (ImGui::Selectable(BLEND_NAME[i], is_selected)) {
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
}