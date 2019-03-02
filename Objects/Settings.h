#pragma once
#include <string>
#include "../ImGUI/TextEditor.h"

namespace ed
{
	class Settings
	{
	public:
		Settings();
		void Load();
		void Save();
		TextEditor::Palette GetTextEditorPalette();

		std::string Theme;

		static inline Settings& Instance()
		{
			static Settings ret;
			return ret;
		}
	};
}