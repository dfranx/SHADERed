#include <SHADERed/UI/Debug/TessellationControlOutputUI.h>
#include <SHADERed/Objects/DefaultState.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/Engine/GLUtils.h>

#include <glm/gtc/type_ptr.hpp>

const char* TESS_CONTROL_OUTPUT_VS_CODE = R"(#version 330

layout (location = 0) in vec2 iPos;

out vec2 posCS;

void main() {
	gl_Position = vec4(iPos.x, iPos.y, 0.0f, 1.0f);
	posCS = iPos;
}
)";

const char* TESS_CONTROL_OUTPUT_PS_CODE = R"(#version 330

out vec4 fragColor;

void main() {
	fragColor = vec4(1.0f);
}
)";

namespace ed {
	DebugTessControlOutputUI::DebugTessControlOutputUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name, bool visible) : 
		UIView(ui, objects, name, visible)
	{
		m_inner = glm::vec2(1.0f);
		m_outer = glm::vec4(1.0f);
		m_error = false;
		m_fbo = m_color = m_depth = 0;
		m_fbo = gl::CreateSimpleFramebuffer(16, 16, m_color, m_depth);
		m_triangleVAO = eng::GeometryFactory::CreateTriangle(m_triangleVBO, 0.5f, gl::CreateDefaultInputLayout());

		m_shader = 0;
		m_buildShaders();
	}
	DebugTessControlOutputUI::~DebugTessControlOutputUI()
	{
		gl::FreeSimpleFramebuffer(m_fbo, m_color, m_depth);
		glDeleteBuffers(1, &m_triangleVBO);
		glDeleteVertexArrays(1, &m_triangleVAO);
		glDeleteShader(m_shader);
	}
	void DebugTessControlOutputUI::OnEvent(const SDL_Event& e)
	{
	}
	void DebugTessControlOutputUI::Update(float delta)
	{
		ImVec2 contentSize = ImGui::GetContentRegionAvail();

		if (contentSize.x != m_lastSize.x || contentSize.y != m_lastSize.y) {
			glBindTexture(GL_TEXTURE_2D, m_color);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, contentSize.x, contentSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glBindTexture(GL_TEXTURE_2D, m_depth);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, contentSize.x, contentSize.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);

			m_lastSize = contentSize;
		}

		m_render();

		ImVec2 cursorPos = ImGui::GetCursorPos();
		ImGui::Image((ImTextureID)m_color, contentSize, ImVec2(0, 1), ImVec2(1, 0));

		ImGui::SetCursorPos(ImVec2(cursorPos.x + 5.0f, cursorPos.y + 5.0f));
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Outer[0] = %.2f\nOuter[1] = %.2f\nOuter[2] = %.2f\nOuter[3] = %.2f\nInner[0] = %.2f\nInner[1] = %.2f",
			m_outer[0], m_outer[1], m_outer[2], m_outer[3], m_inner[0], m_inner[1]);


	}
	void DebugTessControlOutputUI::Refresh()
	{
		glm::vec2 tempInner(1.0f);
		glm::vec4 tempOuter(1.0f);
		bool updated = false;

		// get inner/outer levels
		spvm_word innerMemCount = 0, outerMemCount = 0;
		spvm_member_t innerMem = spvm_state_get_builtin(m_data->Debugger.GetVM(), SpvBuiltInTessLevelInner, &innerMemCount);
		spvm_member_t outerMem = spvm_state_get_builtin(m_data->Debugger.GetVM(), SpvBuiltInTessLevelOuter, &outerMemCount);
		for (spvm_word i = 0; i < std::min<spvm_word>(2, innerMemCount); i++)
			tempInner[i] = std::max<float>(1.0f, innerMem[i].members[0].value.f);
		for (spvm_word i = 0; i < std::min<spvm_word>(4, outerMemCount); i++)
			tempOuter[i] = std::max<float>(1.0f, outerMem[i].members[0].value.f);

		if (tempInner != m_inner || tempOuter != m_outer) {
			m_inner = tempInner;
			m_outer = tempOuter;
			updated = true;
		}

		if (updated)
			m_buildShaders();
	}
	void DebugTessControlOutputUI::m_render()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		static const GLuint fboBuffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, fboBuffers);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
		glClearBufferfv(GL_COLOR, 0, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
		glViewport(0, 0, m_lastSize.x, m_lastSize.y);

		glUseProgram(m_shader);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		glBindVertexArray(m_triangleVAO);
		glDrawArrays(GL_PATCHES, 0, 3);

		glUseProgram(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		DefaultState::Bind();
	}
	void DebugTessControlOutputUI::m_buildShaders()
	{
		GLint success = 0;
		char infoLog[512];

		std::string tessControl = R"(#version 400
layout (vertices = 3) out;

in vec2 posCS[];
out vec2 posES[];

void main()
{
	posES[gl_InvocationID] = posCS[gl_InvocationID];
	
	gl_TessLevelOuter[0] = )" + std::to_string(m_outer[0]) + std::string(R"(;
	gl_TessLevelOuter[1] = )") + std::to_string(m_outer[1]) + std::string(R"(;
	gl_TessLevelOuter[2] = )") + std::to_string(m_outer[2]) + std::string(R"(;
	gl_TessLevelOuter[3] = )") + std::to_string(m_outer[3]) + std::string(R"(;
	gl_TessLevelInner[0] = )") + std::to_string(m_inner[0]) + std::string(R"(;
	gl_TessLevelInner[1] = )") + std::to_string(m_inner[1]) + std::string(R"(;
	
})");
		std::string tessEvaluation = R"(#version 400

layout(triangles, equal_spacing, ccw) in;

in vec2 posES[];

vec2 interpolate(vec2 v0, vec2 v1, vec2 v2)
{
	return v0 * vec2(gl_TessCoord.x) + v1 * vec2(gl_TessCoord.y) + v2 * vec2(gl_TessCoord.z);
}

void main()
{
	vec2 outPos = interpolate(posES[0], posES[1], posES[2]);
	gl_Position = vec4(outPos.x, outPos.y, 0.0, 1.0);
})";


		glDeleteProgram(m_shader);

		// create vertex shader
		unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, &TESS_CONTROL_OUTPUT_VS_CODE, nullptr);
		glCompileShader(vs);
		glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(vs, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile DebugTessControlOutputUI vertex shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create tessellation control shader
		const char* tcsSource = tessControl.c_str();
		unsigned int tcs = glCreateShader(GL_TESS_CONTROL_SHADER);
		glShaderSource(tcs, 1, &tcsSource, nullptr);
		glCompileShader(tcs);
		glGetShaderiv(tcs, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(tcs, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile DebugTessControlOutputUI tessellation control shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create tessellation evaluation shader
		const char* tesSource = tessEvaluation.c_str();
		unsigned int tes = glCreateShader(GL_TESS_EVALUATION_SHADER);
		glShaderSource(tes, 1, &tesSource, nullptr);
		glCompileShader(tes);
		glGetShaderiv(tes, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(tes, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile DebugTessControlOutputUI tessellation evaluation shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create pixel shader
		unsigned int ps = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(ps, 1, &TESS_CONTROL_OUTPUT_PS_CODE, nullptr);
		glCompileShader(ps);
		glGetShaderiv(ps, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(ps, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile DebugTessControlOutputUI pixel shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create a shader program for cubemap preview
		m_shader = glCreateProgram();
		glAttachShader(m_shader, vs);
		glAttachShader(m_shader, tcs);
		glAttachShader(m_shader, tes);
		glAttachShader(m_shader, ps);
		glLinkProgram(m_shader);
		glGetProgramiv(m_shader, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(m_shader, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to create a DebugTessControlOutputUI shader program", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		glDeleteShader(vs);
		glDeleteShader(tcs);
		glDeleteShader(tes);
		glDeleteShader(ps);

	}
}