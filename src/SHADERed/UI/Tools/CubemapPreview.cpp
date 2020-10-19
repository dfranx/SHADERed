#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/UI/Tools/CubemapPreview.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const char* CUBEMAP_VS_CODE = R"(
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

/* https://stackoverflow.com/questions/54101329/project-cubemap-to-2d-texture */
const char* CUBEMAP_PS_CODE = R"(
#version 330

uniform samplerCube cubemap;

in vec2 uv;
out vec4 fragColor;

void main()
{
    vec2 localUV = uv;
	
	localUV.y = mod(localUV.y*3,1);
    localUV.x = mod(localUV.x*4,1);

	vec3 dir = vec3(1.0, 0.0, 0.0);

    if (uv.x*4>1 && uv.x*4<2)
    {
        if (uv.y * 3.f < 1)		// -y
            dir = vec3(localUV.x*2-1,1,localUV.y*2-1);
        else if (uv.y * 3.f > 2)// +y
            dir = vec3(localUV.x*2-1,-1,-localUV.y*2+1);
        else					// -z
            dir = vec3(localUV.x*2-1,-localUV.y*2+1,1);
    }
    else if (uv.y * 3.f > 1 && uv.y * 3.f < 2)
    {
        if (uv.x*4.f < 1)		// -x
            dir=vec3(-1,-localUV.y*2+1,localUV.x*2-1);
        else if (uv.x*4.f < 3)	// +x
            dir=vec3(1,-localUV.y*2+1,-localUV.x*2+1);
        else 					// +z
            dir=vec3(-localUV.x*2+1,-localUV.y*2+1,-1);
    }
    else discard;

    fragColor = texture(cubemap, dir);
}
)";

namespace ed {
	CubemapPreview::~CubemapPreview()
	{
		glDeleteBuffers(1, &m_fsVBO);
		glDeleteVertexArrays(1, &m_fsVAO);
		glDeleteTextures(1, &m_cubeTex);
		glDeleteTextures(1, &m_cubeDepth);
		glDeleteFramebuffers(1, &m_cubeFBO);
	}
	void CubemapPreview::Init(int w, int h)
	{
		Logger::Get().Log("Setting up cubemap preview system...");

		m_w = w;
		m_h = h;

		m_cubeShader = ed::gl::CreateShader(&CUBEMAP_VS_CODE, &CUBEMAP_PS_CODE, "cubemap projection");

		glUseProgram(m_cubeShader);
		m_uMatWVPLoc = glGetUniformLocation(m_cubeShader, "uMatWVP");
		glUniform1i(glGetUniformLocation(m_cubeShader, "cubemap"), 0);
		glUseProgram(0);

		m_fsVAO = ed::eng::GeometryFactory::CreatePlane(m_fsVBO, w, h, gl::CreateDefaultInputLayout());
		m_cubeFBO = gl::CreateSimpleFramebuffer(w, h, m_cubeTex, m_cubeDepth);

		if (m_cubeFBO == 0)
			ed::Logger::Get().Log("Failed to create cubemap preview texture", true);
	}
	void CubemapPreview::Draw(GLuint tex)
	{
		// bind fbo and buffers
		glBindFramebuffer(GL_FRAMEBUFFER, m_cubeFBO);
		static const GLuint fboBuffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, fboBuffers);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
		glClearBufferfv(GL_COLOR, 0, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)));
		glViewport(0, 0, m_w, m_h);
		glUseProgram(m_cubeShader);

		glm::mat4 matVP = glm::ortho(0.0f, m_w, m_h, 0.0f);
		glm::mat4 matW = glm::translate(glm::mat4(1.0f), glm::vec3(m_w / 2, m_h / 2, 0.0f));
		glUniformMatrix4fv(m_uMatWVPLoc, 1, GL_FALSE, glm::value_ptr(matVP * matW));

		// bind shader resource views
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

		glBindVertexArray(m_fsVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
}