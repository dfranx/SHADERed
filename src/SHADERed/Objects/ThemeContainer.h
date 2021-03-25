#pragma once
#include <ImGuiColorTextEdit/TextEditor.h>
#include <imgui/imgui.h>
#include <misc/INIReader.h>
#include <map>
#include <string>

namespace ed {
	struct CustomColors {
		ImVec4 ComputePass;
		ImVec4 ErrorMessage;
		ImVec4 WarningMessage;
		ImVec4 InfoMessage;
	};
	class ThemeContainer {
	public:
		ThemeContainer();

		std::string LoadTheme(const std::string& filename);
		inline const ImGuiStyle& GetUIStyle(const std::string& name)
		{
			return m_ui[name];
		}
		inline const CustomColors& GetCustomStyle(const std::string& name)
		{
			return m_custom[name];
		}
		TextEditor::Palette GetTextEditorStyle(const std::string& name);

		static inline ThemeContainer& Instance()
		{
			static ThemeContainer ret;
			return ret;
		}

	private:
		ImVec4 m_loadColor(const INIReader& ini, const std::string& clr);
		ImVec4 m_parseColor(const std::string& str);

		std::map<std::string, ImGuiStyle> m_ui;
		std::map<std::string, TextEditor::Palette> m_editor;
		std::map<std::string, CustomColors> m_custom;
	};
}