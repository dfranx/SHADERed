#pragma once
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ed {
	class CubemapPreview {
	public:
		~CubemapPreview();

		void Init(int w, int h);
		void Draw(GLuint tex);
		inline GLuint GetTexture() { return m_cubeTex; }

	private:
		float m_w, m_h;

		GLuint m_cubeShader;
		GLuint m_cubeFBO, m_cubeTex, m_cubeDepth;
		GLuint m_fsVAO, m_fsVBO;
		GLuint m_uMatWVPLoc;
	};
}