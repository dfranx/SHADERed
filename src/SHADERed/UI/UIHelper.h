#pragma once
#include <glm/glm.hpp>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

struct spvm_member;
struct spvm_result;
struct spvm_state;

namespace ed {
	class InterfaceManager;

	class UIHelper {
	public:
		static int MessageBox_YesNoCancel(void* window, const std::string& msg);

		static void ShellOpen(const std::string& path);

		static bool Spinner(const char* label, float radius, int thickness, unsigned int color);

		static bool CreateBlendOperatorCombo(const char* name, GLenum& op);
		static bool CreateBlendCombo(const char* name, GLenum& blend);
		static bool CreateCullModeCombo(const char* name, GLenum& cull);
		static bool CreateComparisonFunctionCombo(const char* name, GLenum& comp);
		static bool CreateStencilOperationCombo(const char* name, GLenum& op);
		static bool CreateTextureMinFilterCombo(const char* name, GLenum& filter);
		static bool CreateTextureMagFilterCombo(const char* name, GLenum& filter);
		static bool CreateTextureWrapCombo(const char* name, GLenum& mode);

		static void Markdown(const std::string& md);
	};
}