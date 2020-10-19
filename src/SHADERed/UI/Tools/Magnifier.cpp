#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/DefaultState.h>
#include <SHADERed/UI/Tools/Magnifier.h>

#include <algorithm>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const char* UI_VS_CODE = R"(
#version 330

layout (location = 0) in vec3 iPos;

uniform mat4 uMatWVP;

void main() {
	gl_Position = uMatWVP * vec4(iPos, 1.0f);
}
)";

const char* UI_PS_CODE = R"(
#version 330

out vec4 fragColor;

void main()
{
	fragColor = vec4(0, 0.471f, 0.843f, 0.5f);
}
)";


namespace ed {
	GLuint Magnifier::Shader = 0;
	bool Magnifier::ShaderInitialized = false;

	Magnifier::Magnifier()
	{
		m_mousePos = glm::vec2(0.5f, 0.5f);
		m_pos = glm::vec2(0.0f, 0.0f);
		m_size = glm::vec2(1.0f, 1.0f);
		m_posStart = glm::vec2(0.0f, 0.0f);
		m_zoomDrag = glm::vec2(0.0f, 0.0f);
		m_zoomVAO = m_zoomVBO = 0;
		m_w = 10;
		m_h = 10;
		m_selecting = false;
		m_dragging = false;

		// create a shader program for gizmo
		if (!Magnifier::ShaderInitialized) {
			Magnifier::Shader = gl::CreateShader(&UI_VS_CODE, &UI_PS_CODE, "magnifier");
			Magnifier::ShaderInitialized = true;
		}

		m_zoomShader = Magnifier::Shader;
		m_uMatWVPLoc = glGetUniformLocation(m_zoomShader, "uMatWVP");
	}
	Magnifier::~Magnifier()
	{
		glDeleteBuffers(1, &m_zoomVBO);
		glDeleteVertexArrays(1, &m_zoomVAO);
	}

	void Magnifier::Drag()
	{
		if (m_dragging) {
			float mpy = 1 - m_mousePos.y;
			float mpx = m_mousePos.x;

			float moveY = (mpy - m_zoomDrag.y) * m_size.x;
			float moveX = (m_zoomDrag.x - mpx) * m_size.y;

			m_pos.x = std::max<float>(moveX + m_pos.x, 0.0f);
			m_pos.y = std::max<float>(moveY + m_pos.y, 0.0f);

			m_zoomDrag = glm::vec2(mpx, mpy);

			if (m_pos.x + m_size.x > 1.0f)
				m_pos.x = 1 - m_size.x;
			if (m_pos.y + m_size.y > 1.0f)
				m_pos.y = 1 - m_size.y;
		}
	}
	void Magnifier::StartMouseAction(bool sel)
	{
		m_posStart = m_mousePos;
		m_posStart.y = 1 - m_posStart.y;
		if (sel) {
			m_selecting = true;
			m_dragging = false;
		} else {
			m_dragging = true;
			m_selecting = false;
			m_zoomDrag = m_posStart;
		}
	}
	void Magnifier::EndMouseAction()
	{
		if (m_selecting && m_mousePos.x != m_posStart.x && m_mousePos.y != 1 - m_posStart.y) {
			float mpy = 1 - m_mousePos.y;
			float mpx = m_mousePos.x;

			if (mpy > m_posStart.y) {
				float temp = m_posStart.y;
				m_posStart.y = mpy;
				mpy = temp;
			}
			if (mpx < m_posStart.x) {
				float temp = m_posStart.x;
				m_posStart.x = mpx;
				mpx = temp;
			}

			m_pos.x = m_pos.x + m_size.x * m_posStart.x;
			m_pos.y = m_pos.y + m_size.y * (1 - m_posStart.y);

			glm::vec2 oldSize = m_size;
			m_size.x = (mpx - m_posStart.x) * m_size.x;
			m_size.y = (m_posStart.y - mpy) * m_size.y;
			if (m_size.x >= m_size.y)
				m_size.y = m_size.x;
			if (m_size.y >= m_size.x)
				m_size.x = m_size.y;

			if (m_size.x < (25.0f / m_w) * oldSize.x && m_size.y < (25.0f / m_h) * oldSize.y)
				m_size = oldSize;

			m_size.x = std::max<float>(std::min<float>(m_size.x, 1.0f), 25.0f / m_w);
			m_size.y = std::max<float>(std::min<float>(m_size.y, 1.0f), 25.0f / m_w);

			if (m_pos.x + m_size.x > 1.0f)
				m_pos.x = 1 - m_size.x;
			if (m_pos.y + m_size.y > 1.0f)
				m_pos.y = 1 - m_size.y;
			if (m_pos.x < 0.0f)
				m_pos.x = 0.0f;
			if (m_pos.y < 0.0f)
				m_pos.y = 0.0f;
		}
		m_selecting = false;
		m_dragging = false;
	}
	void Magnifier::Zoom(float w, bool mouseAsPosition)
	{
		float oldZoomWidth = m_size.x, oldZoomHeight = m_size.y;

		m_size.x = std::max<float>(std::min<float>(m_size.x * w, 1.0f), 25.0f / m_w);
		m_size.y = std::max<float>(std::min<float>(m_size.y * w, 1.0f), 25.0f / m_w);

		if (mouseAsPosition) {
			float zx = (m_pos.x + oldZoomWidth * m_mousePos.x) - m_size.x / 2;
			float zy = (m_pos.y + oldZoomHeight * m_mousePos.y) - m_size.y / 2;
			m_pos.x = std::max<float>(zx, 0.0f);
			m_pos.y = std::max<float>(zy, 0.0f);

			if (m_pos.x + m_size.x >= 1.0f)
				m_pos.x = 1.0f - m_size.x;
			if (m_pos.y + m_size.y >= 1.0f)
				m_pos.y = 1.0f - m_size.y;
		} else {
			if (w < 1.0f) {
				m_pos.x += (oldZoomWidth - m_size.x) / 2;
				m_pos.y += (oldZoomHeight - m_size.y) / 2;
			} else {
				float curZoomWidth = m_size.x;
				float curZoomHeight = m_size.y;
				m_size.x = std::min<float>(m_size.x * 2, 1.0f);
				m_size.y = std::min<float>(m_size.y * 2, 1.0f);
				m_pos.x = std::max<float>(m_pos.x - (m_size.x - curZoomWidth) / 2, 0.0f);
				m_pos.y = std::max<float>(m_pos.y - (m_size.y - curZoomHeight) / 2, 0.0f);
			}

			if (m_pos.x + m_size.x >= 1.0f)
				m_pos.x = 1.0f - m_size.x;
			if (m_pos.y + m_size.y >= 1.0f)
				m_pos.y = 1.0f - m_size.y;
		}
	}
	void Magnifier::RebuildVBO(int w, int h)
	{
		m_w = w;
		m_h = h;

		// lines
		std::vector<glm::vec3> verts = {
			{ glm::vec3(0, 0, -1) },
			{ glm::vec3(w, h, -1) },
			{ glm::vec3(0, h, -1) },
			{ glm::vec3(w, 0, -1) },
			{ glm::vec3(w, h, -1) },
			{ glm::vec3(0, 0, -1) },
		};

		if (m_zoomVBO != 0)
			glDeleteBuffers(1, &m_zoomVBO);
		if (m_zoomVAO != 0)
			glDeleteVertexArrays(1, &m_zoomVAO);

		// create vbo
		glGenBuffers(1, &m_zoomVBO);
		glBindBuffer(GL_ARRAY_BUFFER, m_zoomVBO);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);

		// create vao
		glGenVertexArrays(1, &m_zoomVAO);
		glBindVertexArray(m_zoomVAO);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		// unbind
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	void Magnifier::Render()
	{
		DefaultState::Bind();

		glUseProgram(m_zoomShader);

		float mpy = 1 - m_mousePos.y;
		float mpx = m_mousePos.x;
		float zx = m_pos.x + m_size.x * m_posStart.x;
		float zy = 1 - (m_pos.y + m_size.y * (1 - m_posStart.y));
		float zw = (mpx - m_posStart.x) * m_size.x;
		float zh = (mpy - m_posStart.y) * m_size.y;

		glm::mat4 zMatWorld = glm::translate(glm::mat4(1), glm::vec3(zx * m_w, zy * m_h, 0.0f))
			* glm::scale(glm::mat4(1), glm::vec3(zw, zh, 1.0f));

		glm::mat4 zMatProj = glm::ortho(0.0f, (float)m_w, (float)m_h, 0.0f, 0.1f, 100.0f);

		glUniformMatrix4fv(m_uMatWVPLoc, 1, GL_FALSE, glm::value_ptr(zMatProj * zMatWorld));
		glDisable(GL_CULL_FACE);

		glBindVertexArray(m_zoomVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glEnable(GL_CULL_FACE);
	}
}