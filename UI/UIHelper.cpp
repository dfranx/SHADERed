#include "UIHelper.h"
#include "../Objects/Names.h"
#include "../Objects/Logger.h"

#include <clocale>
#include <imgui/imgui.h>
#include <nativefiledialog/nfd.h>

#define HARRAYSIZE(a) (sizeof(a)/sizeof(*a))

namespace ed
{
	bool UIHelper::GetOpenDirectoryDialog(std::string& outPath)
	{
		nfdchar_t* path = NULL;
		nfdresult_t result = NFD_PickFolder(NULL, &path);
		setlocale(LC_ALL, "C");

		outPath = "";
		if (result == NFD_OKAY) {
			outPath = std::string(path);
			return true;
		}
		else if (result == NFD_ERROR)
			ed::Logger::Get().Log("An error occured with file dialog library \"" + std::string(NFD_GetError()) + "\"", true, __FILE__, __LINE__);

		return false;
	}
	bool UIHelper::GetOpenFileDialog(std::string& outPath, const std::string& files)
	{
		nfdchar_t *path = NULL;
		nfdresult_t result = NFD_OpenDialog(NULL, NULL, &path );
		setlocale(LC_ALL,"C");

		outPath = "";
		if ( result == NFD_OKAY ) {
			outPath = std::string(path);
			return true;
		}
		else if ( result == NFD_ERROR)
			ed::Logger::Get().Log("An error occured with file dialog library \"" + std::string(NFD_GetError()) + "\"", true, __FILE__, __LINE__);

		return false;
	}
	bool UIHelper::GetSaveFileDialog(std::string& outPath, const std::string& files)
	{
		nfdchar_t *path = NULL;
		nfdresult_t result = NFD_SaveDialog(files.size() == 0 ? NULL : files.c_str(), NULL, &path );
		setlocale(LC_ALL,"C");

		outPath = "";
		if ( result == NFD_OKAY ) {
			outPath = std::string(path);
			return true;
		}
		else if ( result == NFD_ERROR)
			ed::Logger::Get().Log("An error occured with file dialog library \"" + std::string(NFD_GetError()) + "\"", true, __FILE__, __LINE__);

		return false;
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
	bool UIHelper::CreateCullModeCombo(const char * name, GLenum& cullValue)
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
	bool UIHelper::CreateComparisonFunctionCombo(const char * name, GLenum& compValue)
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
	bool UIHelper::CreateStencilOperationCombo(const char * name, GLenum& opValue)
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
}