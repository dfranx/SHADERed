#include "ShaderVariableContainer.h"
#include "FunctionVariableManager.h"
#include "SystemVariableManager.h"
#include <iostream>

namespace ed
{
	ShaderVariableContainer::ShaderVariableContainer() { }
	ShaderVariableContainer::~ShaderVariableContainer()
	{
		for (int i = 0; i < m_vars.size(); i++) {
			free(m_vars[i]->Data);
			if (m_vars[i]->Arguments != nullptr)
				free(m_vars[i]->Arguments);
			m_vars[i]->Arguments = nullptr;
			delete m_vars[i];
		}
	}
	void ShaderVariableContainer::AddCopy(ShaderVariable var)
	{
		ShaderVariable* n = new ShaderVariable(var);
		m_vars.push_back(n);
	}
	void ShaderVariableContainer::Remove(const char* name)
	{
		for (int i = 0; i < m_vars.size(); i++)
			if (strcmp(m_vars[i]->Name, name) == 0) {
				free(m_vars[i]->Data);
				if (m_vars[i]->Arguments != nullptr)
					free(m_vars[i]->Arguments);
				m_vars[i]->Arguments = nullptr;
				delete m_vars[i];
				m_vars.erase(m_vars.begin() + i);
				break;
			}
	}
	void ShaderVariableContainer::UpdateUniformInfo(GLuint pass)
	{
		GLint count;

		const GLsizei bufSize = 64; // maximum name length
		GLchar name[bufSize]; // variable name in GLSL
		GLsizei length; // name length

		GLint samplerLoc = 0;

		glGetProgramiv(pass, GL_ACTIVE_UNIFORMS, &count);
		for (GLuint i = 0; i < count; i++)
		{
			GLint size;
			GLenum type;

			glGetActiveUniform(pass, (GLuint)i, bufSize, &length, &size, &type, name);

			if (type == GL_SAMPLER_2D) {
				glUniform1i(glGetUniformLocation(pass, name), samplerLoc);
				samplerLoc++;
			}
			else
				m_uLocs[name] = glGetUniformLocation(pass, name);
		}
	}
	void ShaderVariableContainer::Bind()
	{
		for (int i = 0; i < m_vars.size(); i++) {
			if (m_uLocs.count(m_vars[i]->Name) == 0)
				continue;
			
			GLint loc = m_uLocs[m_vars[i]->Name];

			// update values if needed
			SystemVariableManager::Instance().Update(m_vars[i]);
			FunctionVariableManager::Update(m_vars[i]);

			// update uniform every time we bind this container
			// TODO: maybe dont update variables that havent changed
			ShaderVariable::ValueType type = m_vars[i]->GetType();

			switch (type) {
			case ShaderVariable::ValueType::Boolean1:
			case ShaderVariable::ValueType::Integer1:
				glUniform1i(loc, m_vars[i]->AsInteger());
				break;
			case ShaderVariable::ValueType::Boolean2:
			case ShaderVariable::ValueType::Integer2:
				glUniform2iv(loc, 1, m_vars[i]->AsIntegerPtr());
				break;
			case ShaderVariable::ValueType::Boolean3:
			case ShaderVariable::ValueType::Integer3:
				glUniform3iv(loc, 1, m_vars[i]->AsIntegerPtr());
				break;
			case ShaderVariable::ValueType::Boolean4:
			case ShaderVariable::ValueType::Integer4:
				glUniform3iv(loc, 1, m_vars[i]->AsIntegerPtr());
				break;
			case ShaderVariable::ValueType::Float1:
				glUniform1f(loc, m_vars[i]->AsFloat());
				break;
			case ShaderVariable::ValueType::Float2:
				glUniform2fv(loc, 1, m_vars[i]->AsFloatPtr());
				break;
			case ShaderVariable::ValueType::Float3:
				glUniform3fv(loc, 1, m_vars[i]->AsFloatPtr());
				break;
			case ShaderVariable::ValueType::Float4:
				glUniform3fv(loc, 1, m_vars[i]->AsFloatPtr());
				break;
			case ShaderVariable::ValueType::Float2x2:
				glUniformMatrix2fv(loc, 1, GL_FALSE, m_vars[i]->AsFloatPtr());
				break;
			case ShaderVariable::ValueType::Float3x3:
				glUniformMatrix3fv(loc, 1, GL_FALSE, m_vars[i]->AsFloatPtr());
				break;
			case ShaderVariable::ValueType::Float4x4:
				glUniformMatrix4fv(loc, 1, GL_FALSE, m_vars[i]->AsFloatPtr());
				break;
			}
		}
	}
	bool ShaderVariableContainer::ContainsVariable(const char* name)
	{
		for (int i = 0; i < m_vars.size(); i++)
			if (strcmp(m_vars[i]->Name, name) == 0)
				return true;
		return false;
	}
}