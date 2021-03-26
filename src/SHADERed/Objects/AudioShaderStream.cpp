#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/Objects/AudioShaderStream.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

#define SHADER_STREAM_PCM_FRAME_CHUNK_SIZE 1024

void audioShaderCallbackFixed(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	ed::AudioShaderStream* player = (ed::AudioShaderStream*)pDevice->pUserData;
	if (player == NULL)
		return;

	std::lock_guard<std::mutex> guard(player->Mutex);

	player->NeedsUpdate = true;
	memcpy(pOutput, player->GetAudioBuffer(), 1024 * 2 * sizeof(short));
	player->CurrentTime += 1024 / 44100.0f;

	(void)pInput;
}
void audioShaderCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	ed::AudioShaderStream* player = (ed::AudioShaderStream*)pDevice->pUserData;
	if (player == NULL)
		return;

	ma_uint32 pcmFramesAvailableInRB;
	ma_uint32 pcmFramesProcessed = 0;
	ma_uint8* pRunningOutput = (ma_uint8*)pOutput;

	/*
    The first thing to do is check if there's enough data available in the ring buffer. If so we can read from it. Otherwise we need to keep filling
    the ring buffer until there's enough, making sure we only fill the ring buffer in chunks of PCM_FRAME_CHUNK_SIZE.
    */
	while (pcmFramesProcessed < frameCount) { /* Keep going until we've filled the output buffer. */
		ma_uint32 framesRemaining = frameCount - pcmFramesProcessed;

		pcmFramesAvailableInRB = ma_pcm_rb_available_read(player->GetRingBuffer());
		if (pcmFramesAvailableInRB > 0) {
			ma_uint32 framesToRead = (framesRemaining < pcmFramesAvailableInRB) ? framesRemaining : pcmFramesAvailableInRB;
			void* pReadBuffer;

			ma_pcm_rb_acquire_read(player->GetRingBuffer(), &framesToRead, &pReadBuffer);
			{
				memcpy(pRunningOutput, pReadBuffer, framesToRead * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
			}
			ma_pcm_rb_commit_read(player->GetRingBuffer(), framesToRead, pReadBuffer);

			pRunningOutput += framesToRead * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels);
			pcmFramesProcessed += framesToRead;
		} else {
			/*
            There's nothing in the buffer. Fill it with more data from the callback. We reset the buffer first so that the read and write pointers
            are reset back to the start so we can fill the ring buffer in chunks of PCM_FRAME_CHUNK_SIZE which is what we initialized it with. Note
            that this is not how you would want to do it in a multi-threaded environment. In this case you would want to seek the write pointer
            forward via the producer thread and the read pointer forward via the consumer thread (this thread).
            */
			ma_uint32 framesToWrite = SHADER_STREAM_PCM_FRAME_CHUNK_SIZE;
			void* pWriteBuffer;

			ma_pcm_rb_reset(player->GetRingBuffer());
			ma_pcm_rb_acquire_write(player->GetRingBuffer(), &framesToWrite, &pWriteBuffer);
			{
				// MA_ASSERT(framesToWrite == PCM_FRAME_CHUNK_SIZE); /* <-- This should always work in this example because we just reset the ring buffer. */
				audioShaderCallbackFixed(pDevice, pWriteBuffer, NULL, framesToWrite);
			}
			ma_pcm_rb_commit_write(player->GetRingBuffer(), framesToWrite, pWriteBuffer);
		}
	}

	/* Unused in this example. */
	(void)pInput;
}

namespace ed {
	AudioShaderStream::AudioShaderStream()
	{
		m_fboBuffers = GL_COLOR_ATTACHMENT0;

		memset(m_pixels, 0, sizeof(char) * 1024);
		NeedsUpdate = false;
		CurrentTime = 0.0f;

		ma_pcm_rb_init(ma_format_s16, 2, SHADER_STREAM_PCM_FRAME_CHUNK_SIZE, NULL, NULL, &m_rb);
		
		m_deviceConfig = ma_device_config_init(ma_device_type_playback);
		m_deviceConfig.playback.format = ma_format_s16;
		m_deviceConfig.playback.channels = 2;
		m_deviceConfig.sampleRate = 44100;
		m_deviceConfig.dataCallback = audioShaderCallback;
		m_deviceConfig.pUserData = this;

		if (ma_device_init(NULL, &m_deviceConfig, &m_device) != MA_SUCCESS)
			ma_pcm_rb_uninit(&m_rb);
	}
	AudioShaderStream::~AudioShaderStream()
	{
		m_clean();

		gl::FreeSimpleFramebuffer(m_fbo, m_rt, m_depth);
		glDeleteVertexArrays(1, &m_fsRectVAO);
		glDeleteBuffers(1, &m_fsRectVBO);
		glDeleteProgram(m_shader);
	}

	void AudioShaderStream::CompileFromShaderSource(ProjectParser* project, MessageStack* m_msgs, const std::string& str, std::vector<ed::ShaderMacro>& macros, bool isHLSL)
	{
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
		if (isHLSL) {
			std::vector<unsigned int> spv;

			ShaderCompiler::CompileSourceToSPIRV(spv, ed::ShaderLanguage::HLSL, "audio.shader", psCodeIn, ShaderStage::Pixel, "main", macros, m_msgs, project);
			psTrans = ShaderCompiler::ConvertToGLSL(spv, ed::ShaderLanguage::HLSL, ShaderStage::Pixel, false, false, m_msgs);
		}
		const char* psSource = isHLSL ? psTrans.c_str() : psCodeIn.c_str();

		GLint success = 0;
		char infoLog[512];

		// create vertex shader
		unsigned int audioVS = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(audioVS, 1, &vsCode, nullptr);
		glCompileShader(audioVS);

		// create pixel shader
		unsigned int audioPS = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(audioPS, 1, &psSource, nullptr);
		glCompileShader(audioPS);

		// create a shader program for cubemap preview
		m_shader = glCreateProgram();
		glAttachShader(m_shader, audioVS);
		glAttachShader(m_shader, audioPS);
		glLinkProgram(m_shader);

		glDeleteShader(audioVS);
		glDeleteShader(audioPS);

		m_fsRectVAO = ed::eng::GeometryFactory::CreateScreenQuadNDC(m_fsRectVBO, gl::CreateDefaultInputLayout());
		m_fbo = gl::CreateSimpleFramebuffer(1024, 1, m_rt, m_depth, GL_RGBA32F);

		m_svarCurTimeLoc = glGetUniformLocation(m_shader, "sedCurrentTime");
	}
	void AudioShaderStream::RenderAudio()
	{
		if (!NeedsUpdate)
			return;

		std::lock_guard<std::mutex> guard(Mutex);

		glUseProgram(m_shader);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		glDrawBuffers(1, &m_fboBuffers);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
		glClearBufferfv(GL_COLOR, 0, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)));
		glViewport(0, 0, 1024, 1);

		glUniform1f(m_svarCurTimeLoc, CurrentTime);
		glBindVertexArray(m_fsRectVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, m_rt);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, m_pixels);
		glBindTexture(GL_TEXTURE_2D, 0);

		for (int s = 0; s < 1024; s++) {
			int off = s * 4;
			m_audio[s * 2] = m_pixels[off + 0] * INT16_MAX;
			m_audio[s * 2 + 1] = m_pixels[off + 1] * INT16_MAX;
		}

		NeedsUpdate = false;
	}
	void AudioShaderStream::Start()
	{
		if (ma_device_start(&m_device) != MA_SUCCESS)
			m_clean();
	}
	void AudioShaderStream::Stop()
	{
		if (ma_device_stop(&m_device) != MA_SUCCESS)
			m_clean();
	}
	void AudioShaderStream::m_clean()
	{
		ma_device_uninit(&m_device);
		ma_pcm_rb_uninit(&m_rb);
	}
}