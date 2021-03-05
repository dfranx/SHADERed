#pragma once
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ed {
	class Texture3DPreview {
	public:
		~Texture3DPreview();

		void Init();
		void Draw(GLuint tex, int w, int h, float uvZ = 0.0f);
		inline GLuint GetTexture() { return m_color; }

	private:
		float m_w, m_h;

		GLuint m_shader;
		GLuint m_fbo, m_color, m_depth;
		GLuint m_vao, m_vbo;
		GLuint m_uMatWVPLoc, m_uLevelLoc;
	};
}