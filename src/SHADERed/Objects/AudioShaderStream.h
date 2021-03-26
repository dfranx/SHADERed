#pragma once
#include <SHADERed/Objects/MessageStack.h>
#include <SHADERed/Objects/ProjectParser.h>
#include <SHADERed/Objects/ShaderMacro.h>

#include <atomic>
#include <string>
#include <vector>
#include <mutex>

#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <misc/miniaudio.h>

namespace ed {
	class AudioShaderStream {
	public:
		AudioShaderStream();
		~AudioShaderStream();

		void CompileFromShaderSource(ProjectParser* project, MessageStack* msgs, const std::string& str, std::vector<ed::ShaderMacro>& macros, bool isHLSL = false);
		void RenderAudio();

		void Start();
		void Stop();

		inline GLuint GetShader() { return m_shader; }
		inline short* GetAudioBuffer() { return m_audio; }
		inline ma_pcm_rb* GetRingBuffer() { return &m_rb; }

		float CurrentTime;
		std::mutex Mutex;
		std::atomic<bool> NeedsUpdate;

	private:
		void m_clean();

		GLuint m_fboBuffers;
		GLuint m_fsRectVAO, m_fsRectVBO;
		GLuint m_fbo, m_rt, m_depth;
		GLuint m_shader, m_svarCurTimeLoc;

		short m_audio[1024 * 2];
		float m_pixels[1024 * 4];

		ma_device_config m_deviceConfig;
		ma_device m_device;
		ma_pcm_rb m_rb;
	};
}