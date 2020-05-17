#pragma once
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <glm/glm.hpp>

namespace ed {
	class Magnifier {
	public:
		Magnifier();
		~Magnifier();

		static bool ShaderInitialized;
		static GLuint Shader;

		inline bool IsSelecting() { return m_selecting; }
		inline bool IsDragging() { return m_dragging; }

		inline const glm::vec2& GetZoomPosition() { return m_pos; }
		inline const glm::vec2& GetZoomSize() { return m_size; }

		inline void Reset()
		{
			m_pos = glm::vec2(0, 0);
			m_size = glm::vec2(1, 1);
		}

		void StartMouseAction(bool sel);
		void EndMouseAction();
		void Drag();
		void Zoom(float z, bool mouseAsPosition = true);

		inline void SetCurrentMousePosition(glm::vec2 mpos) { m_mousePos = mpos; }
		void RebuildVBO(int w, int h);
		void Render();

	private:
		glm::vec2 m_mousePos;
		glm::vec2 m_pos, m_size;
		glm::vec2 m_posStart, m_zoomDrag; // m_zoomDrag -> m_lastDrag
		int m_w, m_h;
		bool m_selecting;
		bool m_dragging;
		GLuint m_zoomVAO, m_zoomVBO, m_zoomShader, m_uMatWVPLoc;
	};
}