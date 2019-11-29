#pragma once
#include <string>
#include <glm/glm.hpp>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#if defined(__APPLE__)
	#include <OpenGL/gl.h>
#else
	#include <GL/gl.h>
#endif

namespace ed
{
	class UIHelper
	{
	public:
		static bool GetOpenDirectoryDialog(std::string& outPath);
		static bool GetOpenFileDialog(std::string& outPath, const std::string& files = "");
		static bool GetSaveFileDialog(std::string& outPath, const std::string& files = "");

		static bool CreateBlendOperatorCombo(const char* name, GLenum& op);
		static bool CreateBlendCombo(const char* name, GLenum& blend);
		static bool CreateCullModeCombo(const char* name, GLenum& cull);
		static bool CreateComparisonFunctionCombo(const char* name, GLenum& comp);
		static bool CreateStencilOperationCombo(const char* name, GLenum& op);
	};
}