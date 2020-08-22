#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <sstream>

namespace ed {
	ThemeContainer::ThemeContainer()
	{
		m_custom["Light"].ComputePass = ImVec4(0, 0.4f, 0, 1);
		m_custom["Light"].ErrorMessage = ImVec4(1.0f, 0.17f, 0.13f, 1.0f);
		m_custom["Light"].WarningMessage = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
		m_custom["Light"].InfoMessage = ImVec4(0.106f, 0.631f, 0.886f, 1.0f);
		m_custom["Dark"].ComputePass = ImVec4(0, 1.0f, 0.5f, 1);
		m_custom["Dark"].ErrorMessage = ImVec4(1.0f, 0.17f, 0.13f, 1.0f);
		m_custom["Dark"].WarningMessage = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
		m_custom["Dark"].InfoMessage = ImVec4(0.106f, 0.631f, 0.886f, 1.0f);
	}
	ImVec4 ThemeContainer::m_loadColor(const INIReader& ini, const std::string& clrName)
	{
		std::string clr = ini.Get("colors", clrName, "0");
		if (clr == "0") {
			// default colors
			if (clrName == "ComputePass")
				return ImVec4(1, 0, 0, 1);
			else if (clrName == "OutputError")
				return ImVec4(1.0f, 0.17f, 0.13f, 1.0f);
			else if (clrName == "OutputWarning")
				return ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
			else if (clrName == "OutputMessage")
				return ImVec4(0.106f, 0.631f, 0.886f, 1.0f);
		} else
			return m_parseColor(clr);
	}
	std::string ThemeContainer::LoadTheme(const std::string& filename)
	{
		Logger::Get().Log("Loading a theme from file " + filename);

		static const std::string ColorNames[] = {
			    "Text",
				"TextDisabled",
				"WindowBg",
				"ChildBg",
				"PopupBg",
				"Border",
				"BorderShadow",
				"FrameBg",
				"FrameBgHovered",
				"FrameBgActive",
				"TitleBg",
				"TitleBgActive",
				"TitleBgCollapsed",
				"MenuBarBg",
				"ScrollbarBg",
				"ScrollbarGrab",
				"ScrollbarGrabHovered",
				"ScrollbarGrabActive",
				"CheckMark",
				"SliderGrab",
				"SliderGrabActive",
				"Button",
				"ButtonHovered",
				"ButtonActive",
				"Header",
				"HeaderHovered",
				"HeaderActive",
				"Separator",
				"SeparatorHovered",
				"SeparatorActive",
				"ResizeGrip",
				"ResizeGripHovered",
				"ResizeGripActive",
				"Tab",
				"TabHovered",
				"TabActive",
				"TabUnfocused",
				"TabUnfocusedActive",
				"DockingPreview",
				"DockingEmptyBg",
				"PlotLines",
				"PlotLinesHovered",
				"PlotHistogram",
				"PlotHistogramHovered",
				"TableHeaderBg",
				"TableBorderStrong",
				"TableBorderLight",
				"TableRowBg",
				"TableRowBgAlt",
				"TextSelectedBg",
				"DragDropTarget",
				"NavHighlight",
				"NavWindowingHighlight",
				"NavWindowingDimBg",
				"ModalWindowDimBg"
		};
		static const std::string EditorNames[] = {
			"Default",
			"Keyword",
			"Number",
			"String",
			"CharLiteral",
			"Punctuation",
			"Preprocessor",
			"Identifier",
			"KnownIdentifier",
			"PreprocIdentifier",
			"Comment",
			"MultiLineComment",
			"Background",
			"Cursor",
			"Selection",
			"ErrorMarker",
			"Breakpoint",
			"BreakpointOutline",
			"CurrentLineIndicator",
			"CurrentLineIndicatorOutline",
			"LineNumber",
			"CurrentLineFill",
			"CurrentLineFillInactive",
			"CurrentLineEdge",
			"ErrorMessage",
			"BreakpointDisabled",
			"UserFunction",
			"UserType",
			"UniformVariable",
			"GlobalVariable",
			"LocalVariable",
			"FunctionArgument"
		};

		INIReader ini(filename);
		if (ini.ParseError() != 0) {
			Logger::Get().Log("Failed to parse a theme from file " + filename, true);
			return "";
		}

		std::string name = ini.Get("general", "name", "NULL");
		std::string editorTheme = ini.Get("general", "editor", "Dark");
		int version = ini.GetInteger("general", "version", 1);

		CustomColors& custom = m_custom[name];
		ImGuiStyle& style = m_ui[name];
		ImGuiStyle& defaultStyle = ImGui::GetStyle();
		TextEditor::Palette& palette = m_editor[name];

		for (int i = 0; i < ImGuiCol_COUNT; i++) {
			std::string clr = ini.Get("colors", ColorNames[i], "0");

			if (clr == "0")
				style.Colors[(ImGuiCol_)i] = defaultStyle.Colors[(ImGuiCol_)i];
			else
				style.Colors[(ImGuiCol_)i] = m_parseColor(clr);
		}
		custom.ComputePass = m_loadColor(ini, "ComputePass");
		custom.ErrorMessage = m_loadColor(ini, "OutputError");
		custom.WarningMessage = m_loadColor(ini, "OutputWarning");
		custom.InfoMessage = m_loadColor(ini, "OutputMessage");

		style.Alpha = ini.GetReal("style", "Alpha", defaultStyle.Alpha);
		style.WindowPadding.x = ini.GetReal("style", "WindowPaddingX", defaultStyle.WindowPadding.x);
		style.WindowPadding.y = ini.GetReal("style", "WindowPaddingY", defaultStyle.WindowPadding.y);
		style.WindowRounding = ini.GetReal("style", "WindowRounding", defaultStyle.WindowRounding);
		style.WindowBorderSize = ini.GetReal("style", "WindowBorderSize", defaultStyle.WindowBorderSize);
		style.WindowMinSize.x = ini.GetReal("style", "WindowMinSizeX", defaultStyle.WindowMinSize.x);
		style.WindowMinSize.y = ini.GetReal("style", "WindowMinSizeY", defaultStyle.WindowMinSize.y);
		style.WindowTitleAlign.x = ini.GetReal("style", "WindowTitleAlignX", defaultStyle.WindowTitleAlign.x);
		style.WindowTitleAlign.y = ini.GetReal("style", "WindowTitleAlignY", defaultStyle.WindowTitleAlign.y);
		style.ChildRounding = ini.GetReal("style", "ChildRounding", defaultStyle.ChildRounding);
		style.ChildBorderSize = ini.GetReal("style", "ChildBorderSize", defaultStyle.ChildBorderSize);
		style.PopupRounding = ini.GetReal("style", "PopupRounding", defaultStyle.PopupRounding);
		style.PopupBorderSize = ini.GetReal("style", "PopupBorderSize", defaultStyle.PopupBorderSize);
		style.FramePadding.x = ini.GetReal("style", "FramePaddingX", defaultStyle.FramePadding.x);
		style.FramePadding.y = ini.GetReal("style", "FramePaddingY", defaultStyle.FramePadding.y);
		style.FrameRounding = ini.GetReal("style", "FrameRounding", defaultStyle.FrameRounding);
		style.FrameBorderSize = ini.GetReal("style", "FrameBorderSize", defaultStyle.FrameBorderSize);
		style.ItemSpacing.x = ini.GetReal("style", "ItemSpacingX", defaultStyle.ItemSpacing.x);
		style.ItemSpacing.y = ini.GetReal("style", "ItemSpacingY", defaultStyle.ItemSpacing.y);
		style.ItemInnerSpacing.x = ini.GetReal("style", "ItemInnerSpacingX", defaultStyle.ItemInnerSpacing.x);
		style.ItemInnerSpacing.y = ini.GetReal("style", "ItemInnerSpacingY", defaultStyle.ItemInnerSpacing.y);
		style.TouchExtraPadding.x = ini.GetReal("style", "TouchExtraPaddingX", defaultStyle.TouchExtraPadding.x);
		style.TouchExtraPadding.y = ini.GetReal("style", "TouchExtraPaddingY", defaultStyle.TouchExtraPadding.y);
		style.IndentSpacing = ini.GetReal("style", "IndentSpacing", defaultStyle.IndentSpacing);
		style.ColumnsMinSpacing = ini.GetReal("style", "ColumnsMinSpacing", defaultStyle.ColumnsMinSpacing);
		style.ScrollbarSize = ini.GetReal("style", "ScrollbarSize", defaultStyle.ScrollbarSize);
		style.ScrollbarRounding = ini.GetReal("style", "ScrollbarRounding", defaultStyle.ScrollbarRounding);
		style.GrabMinSize = ini.GetReal("style", "GrabMinSize", defaultStyle.GrabMinSize);
		style.GrabRounding = ini.GetReal("style", "GrabRounding", defaultStyle.GrabRounding);
		style.TabRounding = ini.GetReal("style", "TabRounding", defaultStyle.TabRounding);
		style.TabBorderSize = ini.GetReal("style", "TabBorderSize", defaultStyle.TabBorderSize);
		style.ButtonTextAlign.x = ini.GetReal("style", "ButtonTextAlignX", defaultStyle.ButtonTextAlign.x);
		style.ButtonTextAlign.y = ini.GetReal("style", "ButtonTextAlignY", defaultStyle.ButtonTextAlign.y);
		style.DisplayWindowPadding.x = ini.GetReal("style", "DisplayWindowPaddingX", defaultStyle.DisplayWindowPadding.x);
		style.DisplayWindowPadding.y = ini.GetReal("style", "DisplayWindowPaddingY", defaultStyle.DisplayWindowPadding.y);
		style.DisplaySafeAreaPadding.x = ini.GetReal("style", "DisplaySafeAreaPaddingX", defaultStyle.DisplaySafeAreaPadding.x);
		style.DisplaySafeAreaPadding.y = ini.GetReal("style", "DisplaySafeAreaPaddingY", defaultStyle.DisplaySafeAreaPadding.y);
		style.MouseCursorScale = ini.GetReal("style", "MouseCursorScale", defaultStyle.MouseCursorScale);
		style.AntiAliasedLines = ini.GetBoolean("style", "AntiAliasedLines", defaultStyle.AntiAliasedLines);
		style.AntiAliasedFill = ini.GetBoolean("style", "AntiAliasedFill", defaultStyle.AntiAliasedFill);
		style.CurveTessellationTol = ini.GetReal("style", "CurveTessellationTol", defaultStyle.CurveTessellationTol);

		if (editorTheme == "Custom") {
			palette = TextEditor::GetDarkPalette();
			for (int i = 0; i < (int)TextEditor::PaletteIndex::Max; i++) {
				std::string clr = ini.Get("editor", EditorNames[i], "0");
				if (clr == "0") { }
				else {
					ImVec4 c = m_parseColor(clr);
					uint32_t r = c.x * 255;
					uint32_t g = c.y * 255;
					uint32_t b = c.z * 255;
					uint32_t a = c.w * 255;
					palette[i] = (a << 24) | (b << 16) | (g << 8) | r;
				}
			}
		} else if (editorTheme == "Light" || editorTheme == "Dark")
			m_editor[name] = GetTextEditorStyle(editorTheme);

		return name;
	}
	TextEditor::Palette ThemeContainer::GetTextEditorStyle(const std::string& name)
	{
		if (name == "Dark") {
			TextEditor::Palette pal = TextEditor::GetDarkPalette();
			pal[(int)TextEditor::PaletteIndex::Background] = 0x00000000;
			return pal;
		}
		if (name == "Light") {
			TextEditor::Palette pal = TextEditor::GetLightPalette();
			pal[(int)TextEditor::PaletteIndex::Background] = 0x00000000;
			return pal;
		}

		return m_editor[name];
	}
	ImVec4 ThemeContainer::m_parseColor(const std::string& str)
	{
		float res[4] = { 0, 0, 0, 0 };
		int cur = 0;

		std::stringstream ss(str);
		while (ss.good() && cur <= 3) {
			std::string substr;
			std::getline(ss, substr, ',');
			res[cur] = std::stof(substr);

			cur++;
		}

		return ImVec4(res[0], res[1], res[2], res[3]);
	}
}