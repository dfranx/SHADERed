#include <SHADERed/UI/UIHelper.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Names.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <misc/imgui_markdown.h>
#include <SDL2/SDL_messagebox.h>
#include <clocale>
#include <iomanip>
#include <sstream>

#define HARRAYSIZE(a) (sizeof(a) / sizeof(*a))

namespace ed {
	void UIHelper::ShellOpen(const std::string& path)
	{
#if defined(__APPLE__)
		system(("open " + path).c_str());
#elif defined(__linux__) || defined(__unix__)
		system(("xdg-open " + path).c_str());
#elif defined(_WIN32)
		ShellExecuteA(0, 0, path.c_str(), 0, 0, SW_SHOW);
#endif
	}
	bool UIHelper::Spinner(const char* label, float radius, int thickness, unsigned int color) 
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext* g = ImGui::GetCurrentContext();
		const ImGuiStyle& style = g->Style;
		const ImGuiID id = window->GetID(label);

		ImVec2 pos = window->DC.CursorPos;
		ImVec2 size((radius)*2, (radius + style.FramePadding.y) * 2);

		const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
		ImGui::ItemSize(bb, style.FramePadding.y);
		if (!ImGui::ItemAdd(bb, id))
			return false;

		// Render
		window->DrawList->PathClear();

		int num_segments = 30;
		int start = abs(ImSin(g->Time * 1.8f) * (num_segments - 5));

		const float a_min = IM_PI * 2.0f * ((float)start) / (float)num_segments;
		const float a_max = IM_PI * 2.0f * ((float)num_segments - 3) / (float)num_segments;

		const ImVec2 centre = ImVec2(pos.x + radius, pos.y + radius + style.FramePadding.y);

		for (int i = 0; i < num_segments; i++) {
			const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
			window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a + g->Time * 8) * radius,
				centre.y + ImSin(a + g->Time * 8) * radius));
		}

		window->DrawList->PathStroke(color, false, thickness);
	}
	int UIHelper::MessageBox_YesNoCancel(void* window, const std::string& msg)
	{
		const SDL_MessageBoxButtonData buttons[] = {
			{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "CANCEL" },
			{ /* .flags, .buttonid, .text */ 0, 1, "NO" },
			{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "YES" },
		};
		const SDL_MessageBoxData messageboxdata = {
			SDL_MESSAGEBOX_INFORMATION, /* .flags */
			(SDL_Window*)window,		/* .window */
			"SHADERed",					/* .title */
			msg.c_str(),				/* .message */
			SDL_arraysize(buttons),		/* .numbuttons */
			buttons,					/* .buttons */
			NULL						/* .colorScheme */
		};
		int buttonID;
		if (SDL_ShowMessageBox(&messageboxdata, &buttonID) < 0) {
			Logger::Get().Log("Failed to open message box.", true);
			return -1;
		}

		return buttonID;
	}
	bool UIHelper::CreateBlendOperatorCombo(const char* name, GLenum& opValue)
	{
		bool ret = false;
		unsigned int op = 0;
		for (unsigned int i = 0; i < HARRAYSIZE(BLEND_OPERATOR_VALUES); i++)
			if (BLEND_OPERATOR_VALUES[i] == opValue)
				op = i;

		if (ImGui::BeginCombo(name, BLEND_OPERATOR_NAMES[op])) {
			for (int i = 0; i < HARRAYSIZE(BLEND_OPERATOR_NAMES); i++) {
				bool is_selected = ((int)op == i);

				if (strlen(BLEND_OPERATOR_NAMES[i]) > 1)
					if (ImGui::Selectable(BLEND_OPERATOR_NAMES[i], is_selected)) {
						opValue = BLEND_OPERATOR_VALUES[i];
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
	bool UIHelper::CreateBlendCombo(const char* name, GLenum& blendValue)
	{
		bool ret = false;
		unsigned int blend = 0;
		for (unsigned int i = 0; i < HARRAYSIZE(BLEND_VALUES); i++)
			if (BLEND_VALUES[i] == blendValue)
				blend = i;

		if (ImGui::BeginCombo(name, BLEND_NAMES[blend])) {
			for (int i = 0; i < HARRAYSIZE(BLEND_NAMES); i++) {
				bool is_selected = ((int)blend == i);

				if (strlen(BLEND_NAMES[i]) > 1)
					if (ImGui::Selectable(BLEND_NAMES[i], is_selected)) {
						blendValue = BLEND_VALUES[i];
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
	bool UIHelper::CreateCullModeCombo(const char* name, GLenum& cullValue)
	{
		bool ret = false;
		unsigned int cull = 0;
		for (unsigned int i = 0; i < HARRAYSIZE(CULL_MODE_VALUES); i++)
			if (CULL_MODE_VALUES[i] == cullValue)
				cull = i;

		if (ImGui::BeginCombo(name, CULL_MODE_NAMES[cull])) {
			for (int i = 0; i < HARRAYSIZE(CULL_MODE_NAMES); i++) {
				bool is_selected = ((int)cull == i);

				if (strlen(CULL_MODE_NAMES[i]) > 1)
					if (ImGui::Selectable(CULL_MODE_NAMES[i], is_selected)) {
						cullValue = CULL_MODE_VALUES[i];
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
	bool UIHelper::CreateComparisonFunctionCombo(const char* name, GLenum& compValue)
	{
		bool ret = false;
		unsigned int comp = 0;
		for (unsigned int i = 0; i < HARRAYSIZE(COMPARISON_FUNCTION_VALUES); i++)
			if (COMPARISON_FUNCTION_VALUES[i] == compValue)
				comp = i;

		if (ImGui::BeginCombo(name, COMPARISON_FUNCTION_NAMES[comp])) {
			for (int i = 0; i < HARRAYSIZE(COMPARISON_FUNCTION_NAMES); i++) {
				bool is_selected = ((int)comp == i);

				if (strlen(COMPARISON_FUNCTION_NAMES[i]) > 1)
					if (ImGui::Selectable(COMPARISON_FUNCTION_NAMES[i], is_selected)) {
						compValue = COMPARISON_FUNCTION_VALUES[i];
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
	bool UIHelper::CreateStencilOperationCombo(const char* name, GLenum& opValue)
	{
		bool ret = false;
		unsigned int op = 0;
		for (unsigned int i = 0; i < HARRAYSIZE(STENCIL_OPERATION_VALUES); i++)
			if (STENCIL_OPERATION_VALUES[i] == opValue)
				op = i;

		if (ImGui::BeginCombo(name, STENCIL_OPERATION_NAMES[op])) {
			for (int i = 0; i < HARRAYSIZE(STENCIL_OPERATION_NAMES); i++) {
				bool is_selected = ((int)op == i);

				if (strlen(STENCIL_OPERATION_NAMES[i]) > 1)
					if (ImGui::Selectable(STENCIL_OPERATION_NAMES[i], is_selected)) {
						opValue = STENCIL_OPERATION_VALUES[i];
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
	bool UIHelper::CreateTextureMinFilterCombo(const char* name, GLenum& filter)
	{
		bool ret = false;
		unsigned int op = 0;
		for (unsigned int i = 0; i < HARRAYSIZE(TEXTURE_MIN_FILTER_VALUES); i++)
			if (TEXTURE_MIN_FILTER_VALUES[i] == filter)
				op = i;

		if (ImGui::BeginCombo(name, TEXTURE_MIN_FILTER_NAMES[op])) {
			for (int i = 0; i < HARRAYSIZE(TEXTURE_MIN_FILTER_NAMES); i++) {
				bool is_selected = ((int)op == i);

				if (strlen(TEXTURE_MIN_FILTER_NAMES[i]) > 1)
					if (ImGui::Selectable(TEXTURE_MIN_FILTER_NAMES[i], is_selected)) {
						filter = TEXTURE_MIN_FILTER_VALUES[i];
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
	bool UIHelper::CreateTextureMagFilterCombo(const char* name, GLenum& filter)
	{
		bool ret = false;
		unsigned int op = 0;
		for (unsigned int i = 0; i < HARRAYSIZE(TEXTURE_MAG_FILTER_VALUES); i++)
			if (TEXTURE_MAG_FILTER_VALUES[i] == filter)
				op = i;

		if (ImGui::BeginCombo(name, TEXTURE_MAG_FILTER_NAMES[op])) {
			for (int i = 0; i < HARRAYSIZE(TEXTURE_MAG_FILTER_NAMES); i++) {
				bool is_selected = ((int)op == i);

				if (strlen(TEXTURE_MAG_FILTER_NAMES[i]) > 1)
					if (ImGui::Selectable(TEXTURE_MAG_FILTER_NAMES[i], is_selected)) {
						filter = TEXTURE_MAG_FILTER_VALUES[i];
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}
	bool UIHelper::CreateTextureWrapCombo(const char* name, GLenum& mode)
	{
		bool ret = false;
		unsigned int op = 0;
		for (unsigned int i = 0; i < HARRAYSIZE(TEXTURE_WRAP_VALUES); i++)
			if (TEXTURE_WRAP_VALUES[i] == mode)
				op = i;

		if (ImGui::BeginCombo(name, TEXTURE_WRAP_NAMES[op])) {
			for (int i = 0; i < HARRAYSIZE(TEXTURE_WRAP_NAMES); i++) {
				bool is_selected = ((int)op == i);

				if (strlen(TEXTURE_WRAP_NAMES[i]) > 1)
					if (ImGui::Selectable(TEXTURE_WRAP_NAMES[i], is_selected)) {
						mode = TEXTURE_WRAP_VALUES[i];
						ret = true;
					}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return ret;
	}

	void MarkdownLinkCallback(ImGui::MarkdownLinkCallbackData data_)
	{
		std::string url(data_.link, data_.linkLength);
		if (!data_.isImage)
			UIHelper::ShellOpen(url);
	}
	void UIHelper::Markdown(const std::string& md)
	{
		static ImGui::MarkdownConfig mdConfig { MarkdownLinkCallback, NULL, "", { { NULL, true }, { NULL, true }, { NULL, false } } };
		ImGui::Markdown(md.c_str(), md.length(), mdConfig);
	}
}