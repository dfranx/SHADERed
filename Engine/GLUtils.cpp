#include "GLUtils.h"
#include <sstream>
#include <string>

namespace ed
{
	namespace gl
	{
		GLuint CreateSimpleFramebuffer(GLint width, GLint height, GLuint& texColor, GLuint& texDepth, GLuint fmt)
		{
			// create a texture for color information
			glGenTextures(1, &texColor);
			glBindTexture(GL_TEXTURE_2D, texColor);
			glTexImage2D(GL_TEXTURE_2D, 0, fmt, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glBindTexture(GL_TEXTURE_2D, 0);

			glGenTextures(1, &texDepth);
			glBindTexture(GL_TEXTURE_2D, texDepth);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glBindTexture(GL_TEXTURE_2D, 0);

			GLuint ret, actualRet = 1;
			glGenFramebuffers(1, &ret);
			glBindFramebuffer(GL_FRAMEBUFFER, ret);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColor, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texDepth, 0);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				actualRet = 0;

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			return actualRet * ret;
		}
		void FreeSimpleFramebuffer(GLuint& fbo, GLuint& color, GLuint& depth)
		{
			if (color != 0)
				glDeleteTextures(1, &color);

			if (color != 0)
				glDeleteTextures(1, &depth);

			if (fbo != 0)
				glDeleteFramebuffers(1, &fbo);
		}
		GLuint CompileShader(GLenum type, const GLchar* code)
		{
			// TODO: change it to CompileShaders(char* vs, char* ps) and use it everywhere

			// create pixel shader
			GLuint ret = glCreateShader(type);

			// compile pixel shader
			glShaderSource(ret, 1, &code, nullptr);
			glCompileShader(ret);

			return ret;
		}
		bool CheckShaderCompilationStatus(GLuint shader, GLchar* msg)
		{
			GLint ret = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &ret);
			if(!ret)
				glGetShaderInfoLog(shader, 1024, NULL, msg);
			return (bool)ret;
		}

		void CreateVAO(GLuint &geoVAO, GLuint geoVBO, const std::vector<InputLayoutItem> &ilayout, GLuint geoEBO, GLuint bufVBO, std::vector<ed::ShaderVariable::ValueType> types)
		{
			int fmtIndex = 0;

			if (geoVAO == 0)
				glDeleteVertexArrays(1, &geoVAO);

			glGenVertexArrays(1, &geoVAO);
			glBindVertexArray(geoVAO);
			glBindBuffer(GL_ARRAY_BUFFER, geoVBO);

			if (geoEBO != 0)
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geoEBO);

			for (const auto& layitem : ilayout) {
				// vertex positions
				glVertexAttribPointer(fmtIndex, InputLayoutItem::GetValueSize(layitem.Value), GL_FLOAT, GL_FALSE, 18 * sizeof(float), (void *)(InputLayoutItem::GetValueOffset(layitem.Value) * sizeof(GLfloat)));
				glEnableVertexAttribArray(fmtIndex);
				fmtIndex++;
			}

			// user defined
			if (bufVBO != 0) {
				int sizeInBytes = 0;
				for (const auto& fmt : types)
					sizeInBytes += ed::ShaderVariable::GetSize(fmt);

				glBindBuffer(GL_ARRAY_BUFFER, bufVBO);
				int fmtOffset = 0;
				for (const auto& fmt : types) {
					GLint colCount = 0;
					GLenum type = GL_FLOAT;

					switch (fmt) {
						case ShaderVariable::ValueType::Boolean1: colCount = 1; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Boolean2: colCount = 2; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Boolean3: colCount = 3; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Boolean4: colCount = 4; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Integer1: colCount = 1; type = GL_INT; break;
						case ShaderVariable::ValueType::Integer2: colCount = 2; type = GL_INT; break;
						case ShaderVariable::ValueType::Integer3: colCount = 3; type = GL_INT; break;
						case ShaderVariable::ValueType::Integer4: colCount = 4; type = GL_INT; break;
						case ShaderVariable::ValueType::Float1: colCount = 1; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float2: colCount = 2; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float3: colCount = 3; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float4: colCount = 4; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float2x2: colCount = 2; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float3x3: colCount = 3; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float4x4: colCount = 4; type = GL_FLOAT; break;
					}

					glVertexAttribPointer(fmtIndex, colCount, type, GL_FALSE, sizeInBytes, (void*)fmtOffset);
					glEnableVertexAttribArray(fmtIndex);
					glVertexAttribDivisor(fmtIndex, 1);
					
					fmtOffset += ShaderVariable::GetSize(fmt);
					fmtIndex++;
				}
			}

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		std::vector<InputLayoutItem> CreateDefaultInputLayout()
		{
			std::vector<InputLayoutItem> ret;
			ret.push_back({InputLayoutValue::Position, "POSITION"});
			ret.push_back({InputLayoutValue::Normal, "NORMAL"});
			ret.push_back({InputLayoutValue::Texcoord, "TEXCOORD0"});
			return ret;
		}

		std::vector< MessageStack::Message > ParseMessages(const std::string& owner, int shader, const std::string& str, int lineBias)
		{
			std::vector< MessageStack::Message > ret;

			std::istringstream f(str);
			std::string line;
			while (std::getline(f, line)) {
				if (line.find("error") != std::string::npos) {
					size_t firstPar = line.find_first_of('(');
					size_t lastPar = line.find_first_of(')', firstPar);
					int lineNr = std::stoi(line.substr(firstPar+1, lastPar - (firstPar + 1))) - lineBias;
					std::string msg = line.substr(line.find_first_of(':') + 2);
					ret.push_back(MessageStack::Message(MessageStack::Type::Error, owner, msg, lineNr, shader));
				}
			}

			return ret;
		}
		std::vector<MessageStack::Message> ParseHLSLMessages(const std::string& owner, int shader, const std::string& str)
		{
			std::vector< MessageStack::Message > ret;

			std::istringstream f(str);
			std::string line;
			while (std::getline(f, line)) {
				if (line.find("ERROR:") != std::string::npos) {
					size_t firstD = line.find_first_of(':');
					size_t secondD = line.find_first_of(':', firstD+1);
					size_t thirdD = line.find_first_of(':', secondD+1);

					if (firstD == std::string::npos || secondD == std::string::npos || thirdD == std::string::npos)
						continue;

					int lineNr = std::stoi(line.substr(secondD + 1, thirdD - (secondD + 1)));
					std::string msg = line.substr(thirdD + 2);
					ret.push_back(MessageStack::Message(MessageStack::Type::Error, owner, msg, lineNr, shader));
				}
			}

			return ret;
		}
	}
}