#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "../Options.h"

namespace ed
{
	class Settings
	{
	public:
		Settings();
		void Load();
		void Save();

		float DPIScale;		// shouldn't be changed by users
		float TempScale;	// copy this value to DPIScale on "Ok" button press
		std::string Theme;

		struct strGeneral {
			bool VSync;
			// std::string Language;	// [TODO] Not implemented
			bool AutoOpenErrorWindow;
			bool Toolbar;
			bool Recovery;				// [TODO] Not implemented
			bool CheckUpdates;
			bool RecompileOnFileChange;
			bool AutoRecompile;
			bool ReopenShaders;
			bool UseExternalEditor;
			bool OpenShadersOnDblClk;
			bool ItemPropsOnDblCLk;
			bool SelectItemOnDblClk;
			bool Log;
			bool StreamLogs;
			bool PipeLogsToTerminal;
			std::string StartUpTemplate;
			char Font[MAX_PATH];
			int FontSize;
			bool AutoScale;
			std::vector<std::string> HLSLExtensions;
			std::vector<std::string> VulkanGLSLExtensions;
		} General;

		struct strEditor {
			bool SmartPredictions;
			char Font[MAX_PATH];
			int FontSize;
			bool ShowWhitespace;
			bool HiglightCurrentLine;
			bool LineNumbers;
			bool StatusBar;
			bool HorizontalScroll;
			bool AutoBraceCompletion;
			bool SmartIndent;
			bool InsertSpaces;
			int TabSize;
		} Editor;

		struct strPreview {
			bool SwitchLeftRightClick;
			bool HideMenuInPerformanceMode;
			bool BoundingBox;
			bool Gizmo;
			bool GizmoRotationUI;
			int GizmoSnapTranslation; // < 0 -> turned off, > 0 snap value
			int GizmoSnapScale;
			int GizmoSnapRotation;	// in degrees (0 == no snap)
			bool PropertyPick;
			bool StatusBar;
			int FPSLimit;
			bool ApplyFPSLimitToApp; // apply FPSLimit to whole app, not only preview
			bool LostFocusLimitFPS; // limit to 30FPS when app loses focus
			int MSAA; // 1 (off), 2, 4, 8
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

	private:
		void m_parseExt(const std::string& str, std::vector<std::string>& extcontainer);
	};
}