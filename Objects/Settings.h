#pragma once
#include <string>
#include <ImGuiColorTextEdit/TextEditor.h>
#include <MoonLight/Base/Color.h>

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
			bool Recovery;				// [TODO] Not implemented
			bool CheckUpdates;			// [TODO] Not implemented
			bool SupportGLSL;			// [TODO] Not implemented (forgot what it was supposed to do hahah)
			bool RecompileOnFileChange;
			bool ReopenShaders;
			bool OpenShadersOnDblClk;
			std::string StartUpTemplate;
			bool CustomFont;
			char Font[256];
			int FontSize;
			bool AutoScale;
			std::vector<std::string> GLSLExtensions;
		} General;

		struct strEditor {
			bool SmartPredictions;
			char Font[256];
			int FontSize;
			bool ShowWhitespace;			// [TODO] Not implemented
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
			bool FXAA;
			bool SwitchLeftRightClick;
			bool Gizmo;
			bool PropertyPick;
			bool StatusBar;
			int FPSLimit;
		} Preview;

		struct strProject {
			bool FPCamera;
			ml::Color ClearColor;
		} Project;

		static inline Settings& Instance()
		{
			static Settings ret;
			return ret;
		}

	private:
		void m_parseGLSLExt(const std::string& str);
	};
}