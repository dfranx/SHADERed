#pragma once
#include <SHADERed/Engine/Model.h>
#include <glm/glm.hpp>
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
	class GizmoObject {
	public:
		GizmoObject();
		~GizmoObject();

		inline void SetProjectionMatrix(glm::mat4 proj) { m_proj = proj; }
		inline void SetViewMatrix(glm::mat4 view) { m_view = view; }
		inline void SetTransform(glm::vec3* t, glm::vec3* s, glm::vec3* r)
		{
			m_trans = t;
			m_scale = s;
			m_rota = r;
		}

		inline void SetMode(int mode) { m_mode = mode; }

		inline void UnselectAxis() { m_axisSelected = -1; }
		void HandleMouseMove(int x, int y, int vw, int vh);
		int Click(int sx, int sy, int vw, int wh);
		bool Transform(int dx, int dy, bool shift);

		void Render();

	private:
		int m_getBasicAxisSelection(int mx, int my, int vw, int vh, float& depth);

		GLuint m_gizmoShader;

		glm::mat4 m_proj, m_view;
		float m_clickDepth, m_hoverDepth, m_clickDegrees, m_vw, m_vh;
		glm::vec3 m_clickStart, m_hoverStart;
		glm::vec3 *m_trans, *m_scale, *m_rota;
		glm::vec3 m_tValue, m_curValue;
		int m_axisSelected; // -1 = none, 0 = x, 1 = y, 2 = z
		int m_axisHovered;	// ^ same as here
		int m_mode;			// 0 = translation, 1 = scale, 2 = rotation
		float m_lastDepth;

		// 3d model info for gizmo handles
		glm::vec3 m_colors[3];
		eng::Model m_model;

		// ui info
		GLuint m_uiVAO, m_uiVBO, m_uiShader;

		// uniform locations
		GLuint m_uMatWorldLoc, m_uMatVPLoc, m_uColorLoc, m_uMatWVPLoc;
	};
}