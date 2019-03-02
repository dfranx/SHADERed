#pragma once
#include "Settings.h"
#include "../ImGUI/imgui.h"
#include "INIReader.h"
#include <fstream>


namespace ed
{
	Settings::Settings()
	{
		Theme = "Dark";
	}
	void Settings::Load()
	{
		INIReader ini("settings.ini");

		if (ini.ParseError() != 0) return;
		
		Theme = ini.Get("general", "theme", "Dark");
	}
	void Settings::Save()
	{
		std::ofstream ini("settings.ini");
		ini << "[general]" << std::endl;
		ini << "theme=" << Theme << std::endl;
	}
	TextEditor::Palette Settings::GetTextEditorPalette()
	{
		TextEditor::Palette palette = TextEditor::GetDarkPalette();

		if (Theme != "Dark")
			palette = TextEditor::GetLightPalette();

		palette[(int)TextEditor::PaletteIndex::Background] = 0x00000000;

		return palette;
	}
}