#include <SHADERed/Objects/FunctionVariableManager.h>
#include <SHADERed/Objects/Logger.h>
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
		m_clearBlockUBOs();
	}
	void ShaderVariableContainer::m_clearBlockUBOs()
	{
		if (!m_blockUBOs.empty())
			glDeleteBuffers((GLsizei)m_blockUBOs.size(), m_blockUBOs.data());
		m_blockUBOs.clear();
		m_blockMemberOffsets.clear();
		m_blockBindings.clear();
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

		// std140 uniform blocks (e.g. Slang ParameterBlock<T> globals such as time/mouse/
		// custom): their members never show up in GL_ACTIVE_UNIFORMS above, so they need
		// their own enumeration + a real backing UBO to be reachable at all.
		m_clearBlockUBOs();

		GLint blockCount = 0;
		glGetProgramiv(pass, GL_ACTIVE_UNIFORM_BLOCKS, &blockCount);
		for (GLint b = 0; b < blockCount; b++) {
			GLint dataSize = 0;
			glGetActiveUniformBlockiv(pass, b, GL_UNIFORM_BLOCK_DATA_SIZE, &dataSize);
			if (dataSize <= 0)
				continue;

			GLuint ubo = 0;
			glGenBuffers(1, &ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, ubo);
			glBufferData(GL_UNIFORM_BUFFER, dataSize, NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
			m_blockUBOs.push_back(ubo);

			GLuint bindingPoint = (GLuint)m_blockBindings.size();
			glUniformBlockBinding(pass, b, bindingPoint);
			m_blockBindings.push_back({ bindingPoint, ubo });

			GLint activeCount = 0;
			glGetActiveUniformBlockiv(pass, b, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &activeCount);
			if (activeCount <= 0)
				continue;

			std::vector<GLint> indices(activeCount);
			glGetActiveUniformBlockiv(pass, b, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, indices.data());

			std::vector<GLint> offsets(activeCount);
			glGetActiveUniformsiv(pass, activeCount, (const GLuint*)indices.data(), GL_UNIFORM_OFFSET, offsets.data());

			for (GLint m = 0; m < activeCount; m++) {
				GLchar memberName[bufSize];
				GLsizei memberLen;
				GLint memberSize;
				GLenum memberType;
				glGetActiveUniform(pass, (GLuint)indices[m], bufSize, &memberLen, &memberSize, &memberType, memberName);
				m_blockMemberOffsets[memberName] = { ubo, offsets[m] };
			}
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
		// glBindBufferBase binds a global GL binding point, shared by every ComputePass's
		// program - rebind ours every time in case another pass's Bind() stole the same
		// binding point index since our UBOs were set up in UpdateUniformInfo().
		for (auto& bb : m_blockBindings)
			glBindBufferBase(GL_UNIFORM_BUFFER, bb.first, bb.second);

		for (int i = 0; i < m_vars.size(); i++) {
			FunctionVariableManager::Instance().AddToList(m_vars[i]);

			bool isLooseUniform = m_uLocs.count(m_vars[i]->Name) > 0;
			bool isBlockMember = m_blockMemberOffsets.count(m_vars[i]->Name) > 0;
			if (!isLooseUniform && !isBlockMember)
				continue;

			GLint loc = isLooseUniform ? m_uLocs[m_vars[i]->Name] : 0;

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

			if (isBlockMember) {
				// std140 uniform block member: write the raw bytes at their resolved
				// offset into the block's backing UBO. Every type ShaderVariable stores
				// via a system/function binding here is a plain scalar or vector (never
				// a matrix), so its natural size matches its std140 size and no extra
				// padding logic is needed.
				GLuint ubo = m_blockMemberOffsets[m_vars[i]->Name].first;
				GLint offset = m_blockMemberOffsets[m_vars[i]->Name].second;
				int size = ShaderVariable::GetSize(type);
				glBindBuffer(GL_UNIFORM_BUFFER, ubo);
				glBufferSubData(GL_UNIFORM_BUFFER, offset, size, m_vars[i]->Data);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
				continue;
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