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
			bool AutoOpenErrorWindow;	// [TODO] Not implemented
			bool Recovery;				// [TODO] Not implemented
			bool CheckUpdates;			// [TODO] Not implemented
			// std::string Language;	// [TODO] Not implemented
			bool SupportGLSL;			// [TODO] Not implemented
			// OnStartup Startup;		// [TODO] Not implemented
			bool StayOnTop;				// [TODO] Not implemented
			bool ReopenShaders;			// [TODO] Not implemented
		} General;

		struct strEditor {
			bool SmartPredictions;			// [TODO] Not implemented
			std::string Font;				// [TODO] Not implemented 
			bool ShowWhitespace;			// [TODO] Not implemented
			bool HiglightCurrentLine;		// [TODO] Not implemented
			bool LineNumbers;				// [TODO] Not implemented
			bool AutoBraceCompletion;		// [TODO] Not implemented
			char HLSL_Extenstion[12];		// [TODO] Not implemented
			char GLSLVS_Extenstion[12];		// [TODO] Not implemented
			char GLSLPS_Extenstion[12];		// [TODO] Not implemented
			char GLSLGS_Extenstion[12];		// [TODO] Not implemented
			bool SmartIndent;			// [TODO] Not implemented
			bool InsertSpaces;				// [TODO] Not implemented
			int TabSize;
		} Editor;

		struct strPreview {
			bool Gizmo;
			bool PropertyPick;
			bool StatusBar;
		} Preview;

		static inline Settings& Instance()
		{
			static Settings ret;
			return ret;
		}
	};
}