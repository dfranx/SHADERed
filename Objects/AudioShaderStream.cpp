#include "AudioShaderStream.h"
#include "ShaderTranscompiler.h"
#include "../Engine/GeometryFactory.h"
#include "../Engine/GLUtils.h"
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace ed
{
	AudioShaderStream::AudioShaderStream()
	{
		m_fboBuffers = GL_COLOR_ATTACHMENT0;
		m_curTime = 0.0f;
		
		memset(m_pixels, 0, sizeof(char) * 1024);
		m_needsUpdate = false;

		initialize(2, 44100);
	}
	AudioShaderStream::~AudioShaderStream()
	{
		gl::FreeSimpleFramebuffer(m_fbo, m_rt, m_depth);
		glDeleteVertexArrays(1, &m_fsRectVAO);
		glDeleteBuffers(1, &m_fsRectVBO);
		if (m_shader != 0)
			glDeleteShader(m_shader);
		stop();
	}
	
	bool AudioShaderStream::onGetData(Chunk& data)
	{
		m_mutex.lock();

		data.samples = m_audio;
		data.sampleCount = 1024*2;

		m_curTime += 1024.0f/44100.0f;
		m_needsUpdate = true;

		m_mutex.unlock();

		return true;
	}
	void AudioShaderStream::compileFromShaderSource(ProjectParser* project, MessageStack* m_msgs, const std::string& str, std::vector<ed::ShaderMacro>& macros, bool isHLSL)
	{
		if (getStatus() != sf::SoundSource::Status::Playing)
			stop();

		const char* vsCode = R"(
			#version 330
			layout (location = 0) in vec2 pos;
			layout (location = 1) in vec2 uv;

 			void main() {
				gl_Position = vec4(pos, 0.0, 0.0);	
			}
		)";
		std::string psCodeIn = str;
		if (isHLSL) {
			psCodeIn += R"(
				struct PSInput
				{
					float4 Pos : SV_POSITION;
				};
				cbuffer vars : register(b15)
				{
					float sedCurrentTime;
				};
				float4 main(PSInput inp) : SV_TARGET {
					float time = sedCurrentTime + inp.Pos.x / 44100.0f;
					float2 v = mainSound(time);
					return float4(v.x, v.y, 0, 0); // TODO: put 4 samples in one pixel
				}
			)";
		} else {
			psCodeIn += R"(
				out vec4 fragColor;
				uniform float sedCurrentTime;
				void main() {
					float time = sedCurrentTime + gl_FragCoord.x / 44100.0f;
					vec2 v = mainSound(time);
					fragColor = vec4(v.x, v.y, 0, 0); // TODO: put 4 samples in one pixel
				}
			)";
		}
		
		std::string psTrans = "";
		if (isHLSL)
			psTrans = ShaderTranscompiler::TranscompileSource(ed::ShaderLanguage::HLSL, "audio.shader", psCodeIn, 1, "main", macros, false, m_msgs, project);
		const char* psSource = isHLSL ? psTrans.c_str() : psCodeIn.c_str();

		GLint success = 0;
		char infoLog[512];

		// create vertex shader
		unsigned int audioVS = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(audioVS, 1, &vsCode, nullptr);
		glCompileShader(audioVS);
		glGetShaderiv(audioVS, GL_COMPILE_STATUS, &success);
		if(!success && !isHLSL) {
			glGetShaderInfoLog(audioVS, 512, NULL, infoLog);
			m_msgs->Add(gl::ParseMessages(m_msgs->CurrentItem, 2, infoLog));
		}

		// create pixel shader
		unsigned int audioPS = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(audioPS, 1, &psSource, nullptr);
		glCompileShader(audioPS);
		glGetShaderiv(audioPS, GL_COMPILE_STATUS, &success);
		if(!success && !isHLSL) {
			glGetShaderInfoLog(audioPS, 512, NULL, infoLog);
			m_msgs->Add(gl::ParseMessages(m_msgs->CurrentItem, 2, infoLog));
		}

		// create a shader program for cubemap preview
		m_shader = glCreateProgram();
		glAttachShader(m_shader, audioVS);
		glAttachShader(m_shader, audioPS);
		glLinkProgram(m_shader);
		glGetProgramiv(m_shader, GL_LINK_STATUS, &success);
		if(!success && !isHLSL) {
			glGetProgramInfoLog(m_shader, 512, NULL, infoLog);
			m_msgs->Add(gl::ParseMessages(m_msgs->CurrentItem, 2, infoLog));
		}

		glDeleteShader(audioVS);
		glDeleteShader(audioPS);

		m_fsRectVAO = ed::eng::GeometryFactory::CreateScreenQuadNDC(m_fsRectVBO, gl::CreateDefaultInputLayout());
		m_fbo = gl::CreateSimpleFramebuffer(1024, 1, m_rt, m_depth, GL_RGBA32F);

		m_svarCurTimeLoc = glGetUniformLocation(m_shader, "sedCurrentTime");

		if (getStatus() != sf::SoundSource::Status::Playing)
			play();
	}
	void AudioShaderStream::renderAudio()
	{
		if (!m_needsUpdate)
			return;
		
		m_mutex.lock();

		glUseProgram(m_shader);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		glDrawBuffers(1, &m_fboBuffers);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
		glClearBufferfv(GL_COLOR, 0, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)));
		glViewport(0, 0, 1024, 1);

		glUniform1f(m_svarCurTimeLoc, m_curTime);
		glBindVertexArray(m_fsRectVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, m_rt);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, m_pixels);
		glBindTexture(GL_TEXTURE_2D, 0);

		for (int s = 0; s < 1024; s++) {
			int off = s * 4;
			m_audio[s*2] = m_pixels[off + 0] * INT16_MAX;
			m_audio[s*2+1] = m_pixels[off + 1] * INT16_MAX;
		}

		m_needsUpdate = false;
		
		m_mutex.unlock();
	}
	void AudioShaderStream::onSeek(sf::Time timeOffset)
	{
		m_curTime = timeOffset.asSeconds();
	}
}