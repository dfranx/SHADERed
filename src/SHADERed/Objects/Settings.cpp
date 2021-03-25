#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>
#include <misc/INIReader.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace ed {
	Settings::Settings()
	{
		Theme = "Dark";
		LinuxHomeDirectory = "";

		General.VSync = true;
		General.AutoOpenErrorWindow = true;
		General.Toolbar = false;
		General.Profiler = false;
		General.Recovery = false;
		General.CheckUpdates = true;
		General.CheckPluginUpdates = true;
		General.RecompileOnFileChange = false;
		General.AutoRecompile = false;
		General.AutoUniforms = true;
		General.AutoUniformsPin = true;
		General.AutoUniformsFunction = true;
		General.AutoUniformsDelete = true;
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
		General.Tips = false;
		DPIScale = 1.0f;
		strcpy(General.Font, "null");
		General.FontSize = 15;
		m_parseExt("hlsl", General.HLSLExtensions);
		m_parseExt("vk", General.VulkanGLSLExtensions);

		Editor.SmartPredictions = true;
		Editor.ActiveSmartPredictions = false;
		strcpy(Editor.Font, "data/inconsolata.ttf");
		Editor.FontSize = 15;
		Editor.ShowWhitespace = false;
		Editor.HiglightCurrentLine = true;
		Editor.LineNumbers = true;
		Editor.HorizontalScroll = true;
		Editor.StatusBar = false;
		Editor.AutoBraceCompletion = true;
		Editor.SmartIndent = true;
		Editor.AutoIndentOnPaste = false;
		Editor.InsertSpaces = false;
		Editor.TabSize = 4;
		Editor.FunctionTooltips = true;
		Editor.FunctionDeclarationTooltips = false;
		Editor.SyntaxHighlighting = true;
		Editor.ScrollbarMarkers = true;
		Editor.HighlightBrackets = true;
		Editor.CodeFolding = true;

		Debug.ShowValuesOnHover = true;
		Debug.AutoFetch = true;
		Debug.PrimitiveOutline = true;
		Debug.PixelOutline = true;

		Preview.PausedOnStartup = false;
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

		std::string settingsFileLoc = Settings::Instance().ConvertPath("data/settings.ini");

		INIReader ini(settingsFileLoc);

		General.HLSLExtensions.clear();
		General.VulkanGLSLExtensions.clear();

		Theme = ini.Get("general", "theme", "Gray");

		General.VSync = ini.GetBoolean("general", "vsync", true);
		General.AutoOpenErrorWindow = ini.GetBoolean("general", "autoerror", true);
		General.Toolbar = ini.GetBoolean("general", "toolbar", false);
		General.Profiler = ini.GetBoolean("general", "profiler", false);
		General.Recovery = ini.GetBoolean("general", "recovery", false);
		General.CheckUpdates = ini.GetBoolean("general", "checkupdates", true);
		General.CheckPluginUpdates = ini.GetBoolean("general", "checkpluginupdates", true);
		General.Log = ini.GetBoolean("general", "log", false);
		General.StreamLogs = ini.GetBoolean("general", "streamlogs", false);
		General.PipeLogsToTerminal = ini.GetBoolean("general", "pipelogsterminal", false);
		General.ReopenShaders = ini.GetBoolean("general", "reopenshaders", false);
		General.UseExternalEditor = ini.GetBoolean("general", "useexternaleditor", false);
		General.OpenShadersOnDblClk = ini.GetBoolean("general", "openshadersdblclk", true);
		General.ItemPropsOnDblCLk = ini.GetBoolean("general", "itempropsdblclk", true);
		General.SelectItemOnDblClk = ini.GetBoolean("general", "selectitemdblclk", true);
		General.RecompileOnFileChange = ini.GetBoolean("general", "trackfilechange", false);
		General.AutoRecompile = ini.GetBoolean("general", "autorecompile", false);
		General.AutoUniforms = ini.GetBoolean("general", "autouniforms", true);
		General.AutoUniformsPin = ini.GetBoolean("general", "autouniformspin", true);
		General.AutoUniformsFunction = ini.GetBoolean("general", "autouniformsfunction", true);
		General.AutoUniformsDelete = ini.GetBoolean("general", "autouniformsdelete", true);
		General.StartUpTemplate = ini.Get("general", "template", "GLSL");
		General.AutoScale = ini.GetBoolean("general", "autoscale", true);
		General.Tips = ini.GetBoolean("general", "tips", false);
		DPIScale = ini.GetReal("general", "uiscale", 1.0f);
		strcpy(General.Font, ini.Get("general", "font", "data/NotoSans.ttf").c_str());
		General.FontSize = ini.GetInteger("general", "fontsize", 18);
		m_parseExt(ini.Get("general", "hlslext", "hlsl"), General.HLSLExtensions);
		m_parseExt(ini.Get("general", "vkext", "vk"), General.VulkanGLSLExtensions);
		m_parsePluginExt(ini.Get("general", "plext", ""), General.PluginShaderExtensions);

		Editor.SmartPredictions = ini.GetBoolean("editor", "smartpred", true);
		Editor.ActiveSmartPredictions = ini.GetBoolean("editor", "activesmartpred", false);
		strcpy(Editor.Font, ini.Get("editor", "font", "data/inconsolata.ttf").c_str());
		Editor.FontSize = ini.GetInteger("editor", "fontsize", 17);
		Editor.ShowWhitespace = ini.GetBoolean("editor", "whitespace", false);
		Editor.HiglightCurrentLine = ini.GetBoolean("editor", "highlightline", true);
		Editor.LineNumbers = ini.GetBoolean("editor", "linenumbers", true);
		Editor.HorizontalScroll = ini.GetBoolean("editor", "horizscroll", true);
		Editor.StatusBar = ini.GetBoolean("editor", "statusbar", false);
		Editor.AutoBraceCompletion = ini.GetBoolean("editor", "autobrace", true);
		Editor.SmartIndent = ini.GetBoolean("editor", "smartindent", true);
		Editor.AutoIndentOnPaste = ini.GetBoolean("editor", "autoindentonpaste", false);
		Editor.InsertSpaces = ini.GetBoolean("editor", "insertspace", false);
		Editor.TabSize = std::max<int>(std::min<int>(ini.GetInteger("editor", "tabsize", 4), 12), 1);
		Editor.FunctionTooltips = ini.GetBoolean("editor", "functooltips", true);
		Editor.FunctionDeclarationTooltips = ini.GetBoolean("editor", "funcdeclrtooltips", false);
		Editor.SyntaxHighlighting = ini.GetBoolean("editor", "syntaxhighlighting", true);
		Editor.ScrollbarMarkers = ini.GetBoolean("editor", "scrollbarmarkers", true);
		Editor.HighlightBrackets = ini.GetBoolean("editor", "highlightbrackets", true);
		Editor.CodeFolding = ini.GetBoolean("editor", "codefolding", true);

		Debug.ShowValuesOnHover = ini.GetBoolean("debug", "valuesonhover", true);
		Debug.AutoFetch = ini.GetBoolean("debug", "autofetch", true);
		Debug.PixelOutline = ini.GetBoolean("debug", "pixeloutline", true);
		Debug.PrimitiveOutline = ini.GetBoolean("debug", "primitiveoutline", true);

		Preview.PausedOnStartup = ini.GetBoolean("preview", "pausedonstartup", false);
		Preview.SwitchLeftRightClick = ini.GetBoolean("preview", "switchleftrightclick", false);
		Preview.HideMenuInPerformanceMode = ini.GetBoolean("preview", "hidemenuperfmode", true);
		Preview.BoundingBox = ini.GetBoolean("preview", "boundingbox", true);
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

		if (Preview.MSAA != 1 && Preview.MSAA != 2 && Preview.MSAA != 4 && Preview.MSAA != 8 && Preview.MSAA != 16 && Preview.MSAA != 32)
			Preview.MSAA = 1;

		if (Preview.ApplyFPSLimitToApp)
			Preview.LostFocusLimitFPS = false;
	}
	void Settings::Save()
	{
		Logger::Get().Log("Saving settings");

		std::string settingsFileLoc = Settings::Instance().ConvertPath("data/settings.ini");

		std::ofstream ini(settingsFileLoc);

		ini << "[general]" << std::endl;
		ini << "theme=" << Theme << std::endl;
		ini << "vsync=" << General.VSync << std::endl;
		ini << "autoerror=" << General.AutoOpenErrorWindow << std::endl;
		ini << "toolbar=" << General.Toolbar << std::endl;
		ini << "profiler=" << General.Profiler << std::endl;
		ini << "recovery=" << General.Recovery << std::endl;
		ini << "checkupdates=" << General.CheckUpdates << std::endl;
		ini << "checkpluginupdates=" << General.CheckPluginUpdates << std::endl;
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
		ini << "autouniforms=" << General.AutoUniforms << std::endl;
		ini << "autouniformspin=" << General.AutoUniformsPin << std::endl;
		ini << "autouniformsfunction=" << General.AutoUniformsFunction << std::endl;
		ini << "autouniformsdelete=" << General.AutoUniformsDelete << std::endl;
		ini << "template=" << General.StartUpTemplate << std::endl;
		ini << "font=" << General.Font << std::endl;
		ini << "fontsize=" << General.FontSize << std::endl;
		ini << "autoscale=" << General.AutoScale << std::endl;
		ini << "uiscale=" << DPIScale << std::endl;
		ini << "tips=" << General.Tips << std::endl;

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

		ini << "plext=";
		for (const auto& pair : General.PluginShaderExtensions) {
			ini << pair.first << "{";
			for (int i = 0; i < pair.second.size(); i++) {
				ini << pair.second[i];
				if (i != pair.second.size() - 1)
					ini << " ";
			}
			ini << "}";
		}
		ini << std::endl;

		ini << "[preview]" << std::endl;
		ini << "pausedonstartup=" << Preview.PausedOnStartup << std::endl;
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
		ini << "activesmartpred=" << Editor.ActiveSmartPredictions << std::endl;
		ini << "font=" << Editor.Font << std::endl;
		ini << "fontsize=" << Editor.FontSize << std::endl;
		ini << "whitespace=" << Editor.ShowWhitespace << std::endl;
		ini << "highlightline=" << Editor.HiglightCurrentLine << std::endl;
		ini << "linenumbers=" << Editor.LineNumbers << std::endl;
		ini << "horizscroll=" << Editor.HorizontalScroll << std::endl;
		ini << "statusbar=" << Editor.StatusBar << std::endl;
		ini << "autobrace=" << Editor.AutoBraceCompletion << std::endl;
		ini << "smartindent=" << Editor.SmartIndent << std::endl;
		ini << "autoindentonpaste=" << Editor.AutoIndentOnPaste << std::endl;
		ini << "insertspace=" << Editor.InsertSpaces << std::endl;
		ini << "tabsize=" << Editor.TabSize << std::endl;
		ini << "functooltips=" << Editor.FunctionTooltips << std::endl;
		ini << "funcdeclrtooltips=" << Editor.FunctionDeclarationTooltips << std::endl;
		ini << "syntaxhighlighting=" << Editor.SyntaxHighlighting << std::endl;
		ini << "scrollbarmarkers=" << Editor.ScrollbarMarkers << std::endl;
		ini << "highlightbrackets=" << Editor.HighlightBrackets << std::endl;
		ini << "codefolding" << Editor.CodeFolding << std::endl;

		ini << "[debug]" << std::endl;
		ini << "valuesonhover=" << Debug.ShowValuesOnHover << std::endl;
		ini << "autofetch=" << Debug.AutoFetch << std::endl;
		ini << "pixeloutline=" << Debug.PixelOutline << std::endl;
		ini << "primitiveoutline=" << Debug.PrimitiveOutline << std::endl;

		ini << "[plugins]" << std::endl;
		ini << "notloaded=";
		for (int i = 0; i < Plugins.NotLoaded.size(); i++) {
			ini << Plugins.NotLoaded[i];
			if (i != Plugins.NotLoaded.size() - 1)
				ini << " ";
		}
		ini << std::endl;
	}

	std::string Settings::ConvertPath(const std::string& path)
	{
		if (!this->LinuxHomeDirectory.empty())
			return ed::Settings::Instance().LinuxHomeDirectory + path;
		return path;
	}

	void Settings::m_parsePluginExt(const std::string& str, std::unordered_map<std::string, std::vector<std::string>>& extcontainer)
	{
		// Slang{slang, sl}Unity{...}
		std::string cpy = str;

		size_t start = cpy.find_first_of('{');
		while (start != std::string::npos) {
			size_t end = cpy.find_first_of('}', start);

			std::string langName = cpy.substr(0, start);
			std::string exts = cpy.substr(start+1, end-start-1);

			m_parseExt(exts, extcontainer[langName]);

			cpy = cpy.substr(end+1);
			start = cpy.find_first_of('{');
		}
	}
	void Settings::m_parseExt(const std::string& str, std::vector<std::string>& extcontainer)
	{
		std::stringstream ss(str);
		std::string token;

		while (ss >> token) {
			if (token.size() < 1)
				break;

			extcontainer.push_back(token);
		}
	}
}