#pragma once
#include <SHADERed/Options.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <string>
#include <vector>

namespace ed {
	class Settings {
	public:
		Settings();
		void Load();
		void Save();

		// if on linux, check for home directory
		std::string ConvertPath(const std::string& path);

		float DPIScale;	 // shouldn't be changed by users
		float TempScale; // copy this value to DPIScale on "Ok" button press
		std::string Theme;
		std::string LinuxHomeDirectory;

		struct strGeneral {
			bool VSync;
			bool AutoOpenErrorWindow;
			bool Toolbar;
			bool Recovery; // [TODO] Not implemented
			bool Profiler;
			bool CheckUpdates;
			bool CheckPluginUpdates;
			bool RecompileOnFileChange;
			bool AutoRecompile;
			bool AutoUniforms;
			bool AutoUniformsPin;
			bool AutoUniformsFunction;
			bool AutoUniformsDelete;
			bool ReopenShaders;
			bool UseExternalEditor;
			bool OpenShadersOnDblClk;
			bool ItemPropsOnDblCLk;
			bool SelectItemOnDblClk;
			bool Log;
			bool StreamLogs;
			bool PipeLogsToTerminal;
			std::string StartUpTemplate;
			char Font[SHADERED_MAX_PATH];
			int FontSize;
			bool AutoScale;
			bool Tips;
			std::vector<std::string> HLSLExtensions;
			std::vector<std::string> VulkanGLSLExtensions;
			std::unordered_map<std::string, std::vector<std::string>> PluginShaderExtensions;
		} General;

		struct strEditor {
			bool SmartPredictions;
			bool ActiveSmartPredictions;
			char Font[SHADERED_MAX_PATH];
			int FontSize;
			bool ShowWhitespace;
			bool HiglightCurrentLine;
			bool LineNumbers;
			bool StatusBar;
			bool HorizontalScroll;
			bool AutoBraceCompletion;
			bool SmartIndent;
			bool AutoIndentOnPaste;
			bool InsertSpaces;
			bool FunctionDeclarationTooltips;
			bool FunctionTooltips;
			bool SyntaxHighlighting;
			bool ScrollbarMarkers;
			bool HighlightBrackets;
			bool CodeFolding;
			int TabSize;
		} Editor;

		struct strDebug {
			bool ShowValuesOnHover;
			bool AutoFetch;
			bool PrimitiveOutline;
			bool PixelOutline;
		} Debug;

		struct strPreview {
			bool PausedOnStartup;
			bool SwitchLeftRightClick;
			bool HideMenuInPerformanceMode;
			bool BoundingBox;
			bool Gizmo;
			bool GizmoRotationUI;
			int GizmoSnapTranslation; // < 0 -> turned off, > 0 snap value
			int GizmoSnapScale;
			int GizmoSnapRotation; // in degrees (0 == no snap)
			bool PropertyPick;
			bool StatusBar;
			int FPSLimit;
			bool ApplyFPSLimitToApp; // apply FPSLimit to whole app, not only preview
			bool LostFocusLimitFPS;	 // limit to 30FPS when app loses focus
			int MSAA;				 // 1 (off), 2, 4, 8
		} Preview;

		struct strProject {
			bool FPCamera;
			bool UseAlphaChannel;
			glm::vec4 ClearColor;
			std::vector<std::string> IncludePaths;
		} Project;

		struct strPlugins {
			std::vector<std::string> NotLoaded;
		} Plugins;

		static inline Settings& Instance()
		{
			static Settings ret;
			return ret;
		}
		inline float CalculateSize(float h)
		{
			return h * (DPIScale + General.FontSize / 18.0f - 1.0f);
		}
		inline float CalculateWidth(float h)
		{
			return h * (General.FontSize / 18.0f);
		}

	private:
		void m_parseExt(const std::string& str, std::vector<std::string>& extcontainer);
		void m_parsePluginExt(const std::string& str, std::unordered_map<std::string, std::vector<std::string>>& extcontainer);
	};
}