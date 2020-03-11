#include <SHADERed/Objects/DefaultState.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ed {
	void DefaultState::Bind()
	{
		// render states
		glDisable(GL_DEPTH_CLAMP);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);

		// disable blending
		glDisable(GL_BLEND);

		// depth state
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);

		// stencil
		glDisable(GL_STENCIL_TEST);
	}
}
