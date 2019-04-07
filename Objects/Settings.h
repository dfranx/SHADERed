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

		std::string Theme;

		struct strGeneral {
			// std::string Language;	// [TODO] Not implemented
			bool AutoOpenErrorWindow;
			bool Recovery;				// [TODO] Not implemented
			bool CheckUpdates;			// [TODO] Not implemented
			bool SupportGLSL;
			bool ReopenShaders;
			bool OpenShadersOnDblClk;
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
			bool Gizmo;
			bool PropertyPick;
			bool StatusBar;
		} Preview;

		struct strProject {
			bool FPCamera;
			char GLSLVS_Extenstion[12];
			char GLSLPS_Extenstion[12];
			char GLSLGS_Extenstion[12];
		} Project;

		static inline Settings& Instance()
		{
			static Settings ret;
			return ret;
		}
	};
}