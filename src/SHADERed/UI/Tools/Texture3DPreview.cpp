#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/UI/Tools/Texture3DPreview.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const char* TEXTURE3D_PREVIEW_VS_CODE = R"(
#version 330

layout (location = 0) in vec3 iPos;
layout (location = 1) in vec3 iNormal;
layout (location = 2) in vec2 iUV;

uniform mat4 uMatWVP;
out vec2 uv;

void main() {
	gl_Position = uMatWVP * vec4(iPos, 1.0f);
	uv = iUV;
}
)";
const char* TEXTURE3D_PREVIEW_PS_CODE = R"(
#version 330

uniform sampler3D data;
uniform float level;

in vec2 uv;
out vec4 fragColor;

void main()
{
    fragColor = texture(data, vec3(uv.x, uv.y, level));
}
)";

namespace ed {
	Texture3DPreview::~Texture3DPreview()
	{
		glDeleteBuffers(1, &m_vbo);
		glDeleteVertexArrays(1, &m_vao);
		gl::FreeSimpleFramebuffer(m_fbo, m_color, m_depth);
	}
	void Texture3DPreview::Init()
	{
		Logger::Get().Log("Setting up 3D texture preview system...");

		m_shader = ed::gl::CreateShader(&TEXTURE3D_PREVIEW_VS_CODE, &TEXTURE3D_PREVIEW_PS_CODE, "3D texture preview");

		glUseProgram(m_shader);
		m_uMatWVPLoc = glGetUniformLocation(m_shader, "uMatWVP");
		m_uLevelLoc = glGetUniformLocation(m_shader, "level");
		glUniform1i(glGetUniformLocation(m_shader, "data"), 0);
		glUseProgram(0);

		m_w = m_h = 0;

	}
	void Texture3DPreview::Draw(GLuint tex, int w, int h, float uvZ)
	{
		if (m_w != w || m_h != h) {
			glDeleteBuffers(1, &m_vbo);
			glDeleteVertexArrays(1, &m_vao);
			gl::FreeSimpleFramebuffer(m_fbo, m_color, m_depth);
		
			m_vao = ed::eng::GeometryFactory::CreatePlane(m_vbo, w, h, gl::CreateDefaultInputLayout());
			m_fbo = gl::CreateSimpleFramebuffer(w, h, m_color, m_depth);

			m_w = w;
			m_h = h;
		}

		// bind fbo and buffers
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		static const GLuint fboBuffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, fboBuffers);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
		glClearBufferfv(GL_COLOR, 0, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)));
		glViewport(0, 0, m_w, m_h);
		glUseProgram(m_shader);

		glm::mat4 matVP = glm::ortho(0.0f, m_w, m_h, 0.0f);
		glm::mat4 matW = glm::translate(glm::mat4(1.0f), glm::vec3(m_w / 2, m_h / 2, 0.0f));
		glUniformMatrix4fv(m_uMatWVPLoc, 1, GL_FALSE, glm::value_ptr(matVP * matW));
		glUniform1f(m_uLevelLoc, uvZ);

		// bind shader resource views
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D, tex);
		glUniform1i(glGetUniformLocation(m_shader, "data"), 0);

		glBindVertexArray(m_vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
}