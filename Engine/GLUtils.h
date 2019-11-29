#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#if defined(__APPLE__)
	#include <OpenGL/gl.h>
#else
	#include <GL/gl.h>
#endif
#include <string>

#include "../Objects/InputLayout.h"
#include "../Objects/MessageStack.h"
#include "../Objects/ShaderVariable.h"

namespace ed
{
	namespace gl
	{
		GLuint CreateSimpleFramebuffer(GLint width, GLint height, GLuint& texColor, GLuint& texDepth, GLuint fmt = GL_RGBA);
		void FreeSimpleFramebuffer(GLuint& fbo, GLuint& color, GLuint& depth);

		GLuint CompileShader(GLenum type, const GLchar* str);
		bool CheckShaderCompilationStatus(GLuint shader, GLchar* msg);

		std::vector< MessageStack::Message > ParseMessages(const std::string& owner, int shader, const std::string& str, int lineBias = 0);
		std::vector<MessageStack::Message> ParseHLSLMessages(const std::string& owner, int shader, const std::string& str);

		void CreateVAO(GLuint &geoVAO, GLuint geoVBO, const std::vector<InputLayoutItem> &ilayout, GLuint geoEBO = 0, GLuint bufVBO = 0, std::vector<ed::ShaderVariable::ValueType> types = std::vector<ed::ShaderVariable::ValueType>());

		std::vector<InputLayoutItem> CreateDefaultInputLayout();
	}
}