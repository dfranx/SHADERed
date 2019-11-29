#pragma once
#include <SFML/Audio/SoundStream.hpp>
#include <string>
#include <vector>
#include <atomic>
#include <GL/glew.h>
#if defined(__APPLE__)
	#include <OpenGL/gl.h>
#else
	#include <GL/gl.h>
#endif

#include "MessageStack.h"
#include "ShaderMacro.h"
#include "ProjectParser.h"

namespace ed
{
	class AudioShaderStream : public sf::SoundStream
	{
		virtual bool onGetData(Chunk& data);
		virtual void onSeek(sf::Time timeOffset);

	public:
		AudioShaderStream();
		~AudioShaderStream();

		void compileFromShaderSource(ProjectParser* project, MessageStack* msgs, const std::string& str, std::vector<ed::ShaderMacro>& macros, bool isHLSL = false);
		void renderAudio();

		inline GLuint getShader() { return m_shader; }

	private:
		std::atomic<bool> m_needsUpdate;
		sf::Mutex m_mutex;

		float m_curTime;
		GLuint m_fboBuffers;
		GLuint m_fsRectVAO, m_fsRectVBO;
		GLuint m_fbo, m_rt, m_depth;
		GLuint m_shader, m_svarCurTimeLoc;

		sf::Int16 m_audio[1024*2];
		float m_pixels[1024*4];
	};
}