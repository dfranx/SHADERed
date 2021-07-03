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

#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/InputLayout.h>
#include <SHADERed/Objects/MessageStack.h>
#include <SHADERed/Objects/ShaderVariable.h>

namespace ed {
	namespace gl {
		GLuint CreateSimpleFramebuffer(GLint width, GLint height, GLuint& texColor, GLuint& texDepth, GLuint fmt = GL_RGBA);
		void FreeSimpleFramebuffer(GLuint& fbo, GLuint& color, GLuint& depth);

		GLuint CreateShader(const char** vsCode, const char** psCode, const std::string& name = "");

		GLuint CompileShader(GLenum type, const GLchar* str);
		bool CheckShaderCompilationStatus(GLuint shader, GLchar* msg = nullptr);
		bool CheckShaderLinkStatus(GLuint shader, GLchar* msg);

		std::vector<MessageStack::Message> ParseGlslangMessages(const std::string& owner, ShaderStage stage, const std::string& str);

		void CreateBufferVAO(GLuint& geoVAO, GLuint geoVBO, const std::vector<ed::ShaderVariable::ValueType>& ilayout, GLuint bufVBO = 0, std::vector<ed::ShaderVariable::ValueType> types = std::vector<ed::ShaderVariable::ValueType>());
		void CreateVAO(GLuint& geoVAO, GLuint geoVBO, const std::vector<InputLayoutItem>& ilayout, GLuint geoEBO = 0, GLuint bufVBO = 0, std::vector<ed::ShaderVariable::ValueType> types = std::vector<ed::ShaderVariable::ValueType>());

		void GetVertexBufferBounds(ObjectManager* objs, pipe::VertexBuffer* model, glm::vec3& minPosItem, glm::vec3& maxPosItem);

		std::vector<InputLayoutItem> CreateDefaultInputLayout();
	}
}