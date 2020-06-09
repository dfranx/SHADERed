#include <SHADERed/Objects/FunctionVariableManager.h>
#include <SHADERed/Objects/ShaderVariableContainer.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <iostream>
#include <regex>

namespace ed {
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
	ShaderVariable* ShaderVariableContainer::AddCopy(ShaderVariable var)
	{
		ShaderVariable* n = new ShaderVariable(var);
		m_vars.push_back(n);
		return n;
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
		if (pass == 0)
			return;

		GLint count;

		const GLsizei bufSize = 64; // maximum name length
		GLchar name[bufSize];		// variable name in GLSL
		GLsizei length;				// name length
		GLuint samplerLoc = 0;

		m_uLocs.clear();

		glGetProgramiv(pass, GL_ACTIVE_UNIFORMS, &count);
		for (GLuint i = 0; i < count; i++) {
			GLint size;
			GLenum type;

			glGetActiveUniform(pass, (GLuint)i, bufSize, &length, &size, &type, name);

			if (type == GL_SAMPLER_2D)
				glUniform1i(glGetUniformLocation(pass, name), samplerLoc++);
			else
				m_uLocs[name] = glGetUniformLocation(pass, name);
		}
	}
	void ShaderVariableContainer::UpdateTextureList(const std::string& fragShader)
	{
		m_samplers.clear();

		try {
			std::regex re("[a-z]?sampler.+ .+;");
			std::sregex_iterator next(fragShader.begin(), fragShader.end(), re);
			std::sregex_iterator end;
			while (next != end) {
				std::smatch match = *next;
				std::string name = match.str();
				name = name.substr(name.find_first_of(' ') + 1);
				name = name.substr(0, name.size() - 1);
				m_samplers.push_back(name);
				next++;
			}
		} catch (std::regex_error& e) {
			// Syntax error in the regular expression
		}
	}
	void ShaderVariableContainer::UpdateTexture(GLuint pass, GLuint unit)
	{
		if (unit >= m_samplers.size())
			return;

		glUniform1i(glGetUniformLocation(pass, m_samplers[unit].c_str()), unit);
	}
	void ShaderVariableContainer::Bind(void* item)
	{
		for (int i = 0; i < m_vars.size(); i++) {
			FunctionVariableManager::Instance().AddToList(m_vars[i]);

			if (m_uLocs.count(m_vars[i]->Name) == 0)
				continue;

			GLint loc = m_uLocs[m_vars[i]->Name];

			// update values if needed
			SystemVariableManager::Instance().Update(m_vars[i], item);
			FunctionVariableManager::Instance().Update(m_vars[i]);

			// update uniform every time we bind this container
			// TODO: maybe we shouldn't update variables that havent changed
			ShaderVariable::ValueType type = m_vars[i]->GetType();

			// check the flags
			if (m_vars[i]->Flags & (char)ShaderVariable::Flag::Inverse) {
				if (type == ShaderVariable::ValueType::Float4x4) {
					glm::mat4x4 matVal = glm::make_mat4x4(m_vars[i]->AsFloatPtr());
					memcpy(m_vars[i]->Data, glm::value_ptr(glm::inverse(matVal)), sizeof(glm::mat4x4));
				} else if (type == ShaderVariable::ValueType::Float3x3) {
					glm::mat3x3 matVal = glm::make_mat3x3(m_vars[i]->AsFloatPtr());
					memcpy(m_vars[i]->Data, glm::value_ptr(glm::inverse(matVal)), sizeof(glm::mat3x3));
				} else if (type == ShaderVariable::ValueType::Float2x2) {
					glm::mat2x2 matVal = glm::make_mat2x2(m_vars[i]->AsFloatPtr());
					memcpy(m_vars[i]->Data, glm::value_ptr(glm::inverse(matVal)), sizeof(glm::mat2x2));
				}
			}

			switch (type) {
			case ShaderVariable::ValueType::Boolean1:
				glUniform1i(loc, m_vars[i]->AsBoolean());
				break;
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
				glUniform4iv(loc, 1, m_vars[i]->AsIntegerPtr());
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
				glUniform4fv(loc, 1, m_vars[i]->AsFloatPtr());
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