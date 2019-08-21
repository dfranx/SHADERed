#pragma once
#include <string>
#include <ImGuiColorTextEdit/TextEditor.h>
#include <imgui/imgui.h>
#include <map>

namespace ed
{
	class ThemeContainer
	{
	public:
		std::string LoadTheme(const std::string& filename);
		ImGuiStyle GetUIStyle(const std::string& name);
		TextEditor::Palette GetTextEditorStyle(const std::string& name);

		static inline ThemeContainer& Instance()
		{
			static ThemeContainer ret;
			return ret;
		}

	private:
		ImVec4 m_parseColor(const std::string& str);

		std::map<std::string, ImGuiStyle> m_ui;
		std::map<std::string, TextEditor::Palette> m_editor;
	};
}