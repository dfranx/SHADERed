#include "Settings.h"
#include "Logger.h"
#include <inih/INIReader.h>

#include <algorithm>
#include <fstream>
#include <sstream>


namespace ed
{
	Settings::Settings()
	{
		Theme = "Dark";

		General.VSync = false;
		General.AutoOpenErrorWindow = true;
		General.Toolbar = false;
		General.Recovery = false;
		General.CheckUpdates = true;
		General.ReopenShaders = true;
		General.UseExternalEditor = true;
		General.OpenShadersOnDblClk = true;
		General.ItemPropsOnDblCLk = true;
		General.SelectItemOnDblClk = true;
		General.RecompileOnFileChange = true;
		General.StartUpTemplate = "HLSL";
		General.AutoScale = true;
		General.Log = true;
		General.PipeLogsToTerminal = false;
		DPIScale = 1.0f;
		strcpy(General.Font, "null");
		General.FontSize = 15;
		m_parseExt("hlsl", General.HLSLExtensions);
		m_parseExt("vk", General.VulkanGLSLExtensions);

		Editor.SmartPredictions = false;
		strcpy(Editor.Font, "data/inconsolata.ttf");
		Editor.FontSize = 15;
		Editor.ShowWhitespace = false;
		Editor.HiglightCurrentLine = true;
		Editor.LineNumbers = true;
		Editor.HorizontalScroll = true;
		Editor.StatusBar =false;
		Editor.AutoBraceCompletion = true;
		Editor.SmartIndent = true;
		Editor.InsertSpaces = false;
		Editor.TabSize = 4;

		Preview.SwitchLeftRightClick = false;
		Preview.HideMenuInPerformanceMode = false;
		Preview.BoundingBox = false;
		Preview.Gizmo = true;
		Preview.GizmoRotationUI = true;
		Preview.GizmoSnapTranslation = 0;
		Preview.GizmoSnapScale = 0;
		Preview.GizmoSnapRotation = 0;
		Preview.GizmoSnapTranslation = 0;
		Preview.PropertyPick = true;
		Preview.StatusBar = true;
		Preview.FPSLimit = -1;
		Preview.ApplyFPSLimitToApp = false;
		Preview.LostFocusLimitFPS = false;
		Preview.MSAA = 1;
	}
	void Settings::Load()
	{
		Logger::Get().Log("Reading settings");

		INIReader ini("data/settings.ini");

		General.HLSLExtensions.clear();
		General.VulkanGLSLExtensions.clear();

		Theme = ini.Get("general", "theme", "Dark");

		General.VSync = ini.GetBoolean("general", "vsync", false);
		General.AutoOpenErrorWindow = ini.GetBoolean("general", "autoerror", true);
		General.Toolbar = ini.GetBoolean("general", "toolbar", false);
		General.Recovery = ini.GetBoolean("general", "recovery", false);
		General.CheckUpdates = ini.GetBoolean("general", "checkupdates", true);
		General.Log = ini.GetBoolean("general", "log", true);
		General.StreamLogs = ini.GetBoolean("general", "streamlogs", false);
		General.PipeLogsToTerminal = ini.GetBoolean("general", "pipelogsterminal", false);
		General.ReopenShaders = ini.GetBoolean("general", "reopenshaders", true);
		General.UseExternalEditor = ini.GetBoolean("general", "useexternaleditor", true);
		General.OpenShadersOnDblClk = ini.GetBoolean("general", "openshadersdblclk", true);
		General.ItemPropsOnDblCLk = ini.GetBoolean("general", "itempropsdblclk", true);
		General.SelectItemOnDblClk = ini.GetBoolean("general", "selectitemdblclk", true);
		General.RecompileOnFileChange = ini.GetBoolean("general", "trackfilechange", true);
		General.AutoRecompile = ini.GetBoolean("general", "autorecompile", false);
		General.StartUpTemplate = ini.Get("general", "template", "HLSL");
		General.AutoScale = ini.GetBoolean("general", "autoscale", true);
		DPIScale = ini.GetReal("general", "uiscale", 1.0f);
		strcpy(General.Font, ini.Get("general", "font", "null").c_str());
		General.FontSize = ini.GetInteger("general", "fontsize", 15);
		m_parseExt(ini.Get("general", "hlslext", "hlsl"), General.HLSLExtensions);
		m_parseExt(ini.Get("general", "vkext", "vk"), General.VulkanGLSLExtensions);

		Editor.SmartPredictions = ini.GetBoolean("editor", "smartpred", false);
		strcpy(Editor.Font, ini.Get("editor", "font", "data/inconsolata.ttf").c_str());
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

		Preview.SwitchLeftRightClick = ini.GetBoolean("preview", "switchleftrightclick", false);
		Preview.HideMenuInPerformanceMode = ini.GetBoolean("preview", "hidemenuperfmode", false);
		Preview.BoundingBox = ini.GetBoolean("preview", "boundingbox", false);
		Preview.Gizmo = ini.GetBoolean("preview", "gizmo", true);
		Preview.GizmoRotationUI = ini.GetBoolean("preview", "gizmorotaui", true);
		Preview.GizmoSnapTranslation = ini.GetInteger("preview", "gizmosnaptrans", 0);
		Preview.GizmoSnapScale = ini.GetInteger("preview", "gizmosnapscale", 0);
		Preview.GizmoSnapRotation = ini.GetInteger("preview", "gizmosnaprota", 0);
		Preview.GizmoSnapTranslation = ini.GetInteger("preview", "gizmosnaptrans", 0);
		Preview.PropertyPick = ini.GetBoolean("preview", "propertypick", true);
		Preview.StatusBar = ini.GetBoolean("preview", "statusbar", true);
		Preview.FPSLimit = ini.GetInteger("preview", "fpslimit", -1);
		Preview.ApplyFPSLimitToApp = ini.GetBoolean("preview", "fpslimitwholeapp", false);
		Preview.LostFocusLimitFPS = ini.GetBoolean("preview", "fpslimitlostfocus", false);
		Preview.MSAA = ini.GetInteger("preview", "msaa", 1);

		m_parseExt(ini.Get("plugins", "notloaded", ""), Plugins.NotLoaded);
		
		if (Preview.MSAA != 1 && Preview.MSAA != 2 && Preview.MSAA != 4 &&
			Preview.MSAA != 8 && Preview.MSAA != 16 && Preview.MSAA != 32)
			Preview.MSAA = 1;

		if (Preview.ApplyFPSLimitToApp)
			Preview.LostFocusLimitFPS = false;
	}
	void Settings::Save()
	{
		Logger::Get().Log("Saving settings");
		
		std::ofstream ini("data/settings.ini");

		ini << "[general]" << std::endl;
		ini << "theme=" << Theme << std::endl;
		ini << "vsync=" << General.VSync << std::endl;
		ini << "autoerror=" << General.AutoOpenErrorWindow << std::endl;
		ini << "toolbar=" << General.Toolbar << std::endl;
		ini << "recovery=" << General.Recovery << std::endl;
		ini << "checkupdates=" << General.CheckUpdates << std::endl;
		ini << "log=" << General.Log << std::endl;
		ini << "streamlogs=" << General.StreamLogs << std::endl;
		ini << "pipelogsterminal=" << General.PipeLogsToTerminal << std::endl;
		ini << "reopenshaders=" << General.ReopenShaders << std::endl;
		ini << "useexternaleditor=" << General.UseExternalEditor << std::endl;
		ini << "openshadersdblclk=" << General.OpenShadersOnDblClk << std::endl;
		ini << "itempropsdblclk=" << General.ItemPropsOnDblCLk << std::endl;
		ini << "selectitemdblclk=" << General.SelectItemOnDblClk << std::endl;
		ini << "trackfilechange=" << General.RecompileOnFileChange << std::endl;
		ini << "autorecompile=" << General.AutoRecompile << std::endl;
		ini << "template=" << General.StartUpTemplate << std::endl;
		ini << "font=" << General.Font << std::endl;
		ini << "fontsize=" << General.FontSize << std::endl;
		ini << "autoscale=" << General.AutoScale << std::endl;
		ini << "uiscale=" << DPIScale << std::endl;
		
		ini << "hlslext=";
		for (int i = 0; i < General.HLSLExtensions.size(); i++) {
			ini << General.HLSLExtensions[i];
			if (i != General.HLSLExtensions.size() - 1)
				ini << " ";
		}
		ini << std::endl;

		ini << "vkext=";
		for (int i = 0; i < General.VulkanGLSLExtensions.size(); i++) {
			ini << General.VulkanGLSLExtensions[i];
			if (i != General.VulkanGLSLExtensions.size() - 1)
				ini << " ";
		}
		ini << std::endl;

		ini << "[preview]" << std::endl;
		ini << "switchleftrightclick=" << Preview.SwitchLeftRightClick << std::endl;
		ini << "hidemenuperfmode=" << Preview.HideMenuInPerformanceMode << std::endl;
		ini << "boundingbox=" << Preview.BoundingBox << std::endl;
		ini << "gizmo=" << Preview.Gizmo << std::endl;
		ini << "gizmorotaui=" << Preview.GizmoRotationUI << std::endl;
		ini << "gizmosnaptrans=" << Preview.GizmoSnapTranslation << std::endl;
		ini << "gizmosnapscale=" << Preview.GizmoSnapScale << std::endl;
		ini << "gizmosnaprota=" << Preview.GizmoSnapRotation << std::endl;
		ini << "propertypick=" << Preview.PropertyPick << std::endl;
		ini << "statusbar=" << Preview.StatusBar << std::endl;
		ini << "fpslimit=" << Preview.FPSLimit << std::endl;
		ini << "fpslimitwholeapp=" << Preview.ApplyFPSLimitToApp << std::endl;
		ini << "fpslimitlostfocus=" << Preview.LostFocusLimitFPS << std::endl;
		ini << "msaa=" << Preview.MSAA << std::endl;

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


		ini << "[plugins]" << std::endl;
		ini << "notloaded=";
		for (int i = 0; i < Plugins.NotLoaded.size(); i++) {
			ini << Plugins.NotLoaded[i];
			if (i != Plugins.NotLoaded.size() - 1)
				ini << " ";
		}
		ini << std::endl;


	}

	void Settings::m_parseExt(const std::string& str, std::vector<std::string>& extcontainer)
	{
		std::stringstream ss(str);
		std::string token;

		while (ss >> token)
		{
			if (token.size() < 1)
				break;

			extcontainer.push_back(token);
		}
	}
}