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
	};
}