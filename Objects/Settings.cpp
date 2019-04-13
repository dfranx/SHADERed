#pragma once
#include "Settings.h"
#include "../ImGUI/imgui.h"
#include "INIReader.h"

#include <algorithm>
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

		General.VSync = ini.GetBoolean("general", "vsync", false);
		General.AutoOpenErrorWindow = ini.GetBoolean("general", "autoerror", true);
		General.Recovery = ini.GetBoolean("general", "recovery", false);
		General.CheckUpdates = ini.GetBoolean("general", "checkupdates", true);
		General.SupportGLSL = ini.GetBoolean("general", "glsl", true);
		General.ReopenShaders = ini.GetBoolean("general", "reopenshaders", true);
		General.OpenShadersOnDblClk = ini.GetBoolean("general", "openshadersdblclk", true);
		General.StartUpTemplate = ini.Get("general", "template", "HLSL");

		Editor.SmartPredictions = ini.GetBoolean("editor", "smartpred", false);
		strcpy(Editor.Font, ini.Get("editor", "font", "inconsolata.ttf").c_str());
		Editor.FontSize = ini.GetInteger("editor", "fontsize", 15);
		Editor.ShowWhitespace = ini.GetBoolean("editor", "whitespace", false);
		Editor.HiglightCurrentLine = ini.GetBoolean("editor", "highlightline", true);
		Editor.LineNumbers = ini.GetBoolean("editor", "linenumbers", true);
		Editor.HorizontalScroll = ini.GetBoolean("editor", "horizscroll", true);
		Editor.StatusBar = ini.GetBoolean("editor", "statusbar", false);
		Editor.AutoBraceCompletion = ini.GetBoolean("editor", "autobrace", true);
		Editor.SmartIndent = ini.GetBoolean("editor", "smartindent", true);
		Editor.InsertSpaces = ini.GetBoolean("editor", "insertspace", false);
		Editor.TabSize = std::max<int>(std::min<int>(ini.GetInteger("editor", "tabsize", 4), 12), 1);

		Preview.Gizmo = ini.GetBoolean("preview", "gizmo", true);
		Preview.PropertyPick = ini.GetBoolean("preview", "propertypick", true);
		Preview.StatusBar = ini.GetBoolean("preview", "statusbar", true);
		Preview.FPSLimit = ini.GetInteger("preview", "fpslimit", -1);
	}
	void Settings::Save()
	{
		std::ofstream ini("settings.ini");

		ini << "[general]" << std::endl;
		ini << "theme=" << Theme << std::endl;
		ini << "vsync=" << General.VSync << std::endl;
		ini << "autoerror=" << General.AutoOpenErrorWindow << std::endl;
		ini << "recovery=" << General.Recovery << std::endl;
		ini << "checkupdates=" << General.CheckUpdates << std::endl;
		ini << "glsl=" << General.SupportGLSL << std::endl;
		ini << "reopenshaders=" << General.ReopenShaders << std::endl;
		ini << "openshadersdblclk=" << General.OpenShadersOnDblClk << std::endl;
		ini << "template=" << General.StartUpTemplate << std::endl;

		ini << "[preview]" << std::endl;
		ini << "gizmo=" << Preview.Gizmo << std::endl;
		ini << "propertypick=" << Preview.PropertyPick << std::endl;
		ini << "statusbar=" << Preview.StatusBar << std::endl;
		ini << "fpslimit=" << Preview.FPSLimit << std::endl;

		ini << "[editor]" << std::endl;
		ini << "smartpred=" << Editor.SmartPredictions << std::endl;
		ini << "font=" << Editor.Font << std::endl;
		ini << "fontsize=" << Editor.FontSize << std::endl;
		ini << "whitespace=" << Editor.ShowWhitespace << std::endl;
		ini << "highlightline=" << Editor.HiglightCurrentLine << std::endl;
		ini << "linenumbers=" << Editor.LineNumbers << std::endl;
		ini << "horizscroll=" << Editor.HorizontalScroll << std::endl;
		ini << "statusbar=" << Editor.StatusBar << std::endl;
		ini << "autobrace=" << Editor.AutoBraceCompletion << std::endl;
		ini << "smartindent=" << Editor.SmartIndent << std::endl;
		ini << "insertspace=" << Editor.InsertSpaces << std::endl;
		ini << "tabsize=" << Editor.TabSize << std::endl;

	}
}