#include "UIHelper.h"
#include "../ImGUI/imgui.h"

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
}