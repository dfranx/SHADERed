#pragma once
#include <SHADERed/Objects/ShaderVariable.h>
#include <map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ed {
	class ShaderVariableContainer {
	public:
		ShaderVariableContainer();
		~ShaderVariableContainer();

		inline void Add(ShaderVariable* var) { m_vars.push_back(var); }
		ShaderVariable* AddCopy(ShaderVariable var);
		void Remove(const char* name);

		bool ContainsVariable(const char* name);
		void UpdateUniformInfo(GLuint pass);
		void UpdateTexture(GLuint pass, GLuint unit);
		void UpdateTextureList(const std::string& fragShader);
		void Bind(void* item = nullptr);
		inline std::vector<ShaderVariable*>& GetVariables() { return m_vars; }
		inline const std::vector<std::string>& GetSamplerList() { return m_samplers; }

	private:
		std::vector<ShaderVariable*> m_vars;
		std::map<std::string, GLint> m_uLocs;
		std::vector<std::string> m_samplers;

		// uniform block member support (e.g. Slang ParameterBlock<T> globals such as
		// time/mouse/custom, which compile to a named std140 uniform block instead of
		// loose default-block uniforms and so are invisible to GL_ACTIVE_UNIFORMS).
		std::map<std::string, std::pair<GLuint, GLint>> m_blockMemberOffsets; // full name -> (ubo, byte offset)
		std::vector<GLuint> m_blockUBOs;
		std::vector<std::pair<GLuint, GLuint>> m_blockBindings; // (binding point, ubo) - rebound every Bind() since
		                                                        // glBindBufferBase is global GL state shared by
		                                                        // every ComputePass program, not per-program
		void m_clearBlockUBOs();
	};
}