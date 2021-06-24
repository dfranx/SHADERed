#include <SHADERed/UI/Debug/VectorWatchUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Objects/DefaultState.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <imgui/imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

const char* SIMPLE_VECTOR_VS_CODE = R"(
#version 330

layout (location = 0) in vec3 iPos;

uniform mat4 uMatVP;
uniform mat4 uMatWorld;
uniform vec4 uColor;

out vec4 oColor;

void main() {
	gl_Position = uMatVP * uMatWorld * vec4(iPos, 1.0f);
	oColor = uColor;
}
)";

const char* SIMPLE_VECTOR_PS_CODE = R"(
#version 330

in vec4 oColor;
out vec4 fragColor;

void main() {
	fragColor = oColor;
}
)";

namespace ed {
	GLuint createLines(GLuint& vbo, const std::vector<float>& data)
	{
		GLuint vao;

		// vbo
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(GLfloat), data.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// vao
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		return vao;
	}
	glm::mat3 rotateAlign(glm::vec3 u1, glm::vec3 u2)
	{
		glm::vec3 axis = glm::cross(u1, u2);

		const float cosA = glm::dot(u1, u2);
		const float k = 1.0f / (1.0f + cosA);

		glm::mat3 result((axis.x * axis.x * k) + cosA,
			(axis.y * axis.x * k) - axis.z,
			(axis.z * axis.x * k) + axis.y,
			(axis.x * axis.y * k) + axis.z,
			(axis.y * axis.y * k) + cosA,
			(axis.z * axis.y * k) - axis.x,
			(axis.x * axis.z * k) - axis.y,
			(axis.y * axis.z * k) + axis.x,
			(axis.z * axis.z * k) + cosA);

		return result;
	}

	DebugVectorWatchUI::~DebugVectorWatchUI()
	{
		glDeleteBuffers(1, &m_unitSphereVBO);
		glDeleteVertexArrays(1, &m_unitSphereVAO);
		glDeleteBuffers(1, &m_gridVBO);
		glDeleteVertexArrays(1, &m_gridVAO);
		glDeleteBuffers(1, &m_xVBO);
		glDeleteVertexArrays(1, &m_xVAO);
		glDeleteBuffers(1, &m_yVBO);
		glDeleteVertexArrays(1, &m_yVAO);
		glDeleteBuffers(1, &m_zVBO);
		glDeleteVertexArrays(1, &m_zVAO);
		glDeleteProgram(m_simpleShader);
		glDeleteTextures(1, &m_fboColor);
		glDeleteTextures(1, &m_fboDepth);
		glDeleteFramebuffers(1, &m_fbo);
	}
	void DebugVectorWatchUI::m_init()
	{
		m_simpleShader = gl::CreateShader(&SIMPLE_VECTOR_VS_CODE, &SIMPLE_VECTOR_PS_CODE, "vector watch");

		m_lastFBOWidth = m_lastFBOHeight = 512.0f;
		m_fbo = gl::CreateSimpleFramebuffer(512, 512, m_fboColor, m_fboDepth, GL_RGB);

		std::vector<InputLayoutItem> layout;
		layout.push_back({ InputLayoutValue::Position, "POSITION" });

		m_unitSphereVAO = eng::GeometryFactory::CreateSphere(m_unitSphereVBO, 1.0f, layout);

		m_uMatWorld = glGetUniformLocation(m_simpleShader, "uMatWorld");
		m_uMatVP = glGetUniformLocation(m_simpleShader, "uMatVP");
		m_uColor = glGetUniformLocation(m_simpleShader, "uColor");

		// create the grid
		std::vector<float> gridData(160*2*3);
		// vertices
		int fix = 0;
		for (int x = 0; x < 80; x++) {
			if (x == 40) {
				fix = 1;
				continue;
			}

			int index = (x-fix) * 6;
			// start (x,y,z)
			gridData[index + 0] = x - 40;
			gridData[index + 1] = 0.0f;
			gridData[index + 2] = -40.0f;
			// end (x,y,z)
			gridData[index + 3] = x - 40;
			gridData[index + 4] = 0.0f;
			gridData[index + 5] = 40.0f;
		}
		fix = 0;
		for (int y = 0; y < 81; y++) {
			if (y == 40) {
				fix = 1;
				continue;
			}

			int index = 80*2*3 + (y-fix) * 6;
			// start (x,y,z)
			gridData[index + 0] = -40.0f;
			gridData[index + 1] = 0.0f;
			gridData[index + 2] = y - 40;
			// end (x,y,z)
			gridData[index + 3] = 40.0f;
			gridData[index + 4] = 0.0f;
			gridData[index + 5] = y - 40;
		}
		m_gridVAO = createLines(m_gridVBO, gridData);

		gridData.resize(2 * 3);

		// x line
		gridData[0] = -40.0f;
		gridData[1] = 0.0f;
		gridData[2] = 0.0f;
		gridData[3] = 40.0f;
		gridData[4] = 0.0f;
		gridData[5] = 0.0f;
		m_xVAO = createLines(m_xVBO, gridData);

		// y line
		gridData[0] = 0.0f;
		gridData[1] = 400.0f;
		gridData[2] = 0.0f;
		gridData[3] = 0.0f;
		gridData[4] = -400.0f;
		gridData[5] = 0.0f;
		m_yVAO = createLines(m_yVBO, gridData);

		// z line
		gridData[0] = 0.0f;
		gridData[1] = 0.0f;
		gridData[2] = 40.0f;
		gridData[3] = 0.0f;
		gridData[4] = 0.0f;
		gridData[5] = -40.0f;
		m_zVAO = createLines(m_zVBO, gridData);

		// arrow
		m_vectorHandle.LoadFromFile("data/vector_handle.obj");
		m_vectorPoint.LoadFromFile("data/vector_point.obj");

		m_vectorPointSize = m_vectorPoint.GetMaxBound().z - m_vectorPoint.GetMinBound().z;
	}
	void DebugVectorWatchUI::m_render()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		static const GLuint fboBuffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, fboBuffers);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
		glClearBufferfv(GL_COLOR, 0, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)));
		glViewport(0, 0, m_lastFBOWidth, m_lastFBOHeight);

		glUseProgram(m_simpleShader);

		glm::mat4 matWorld = glm::mat4(1.0f);
		glm::mat4 matVP = glm::perspective(glm::radians(45.0f), m_lastFBOWidth / m_lastFBOHeight, 0.1f, 1000.0f) * m_camera.GetMatrix();

		glUniformMatrix4fv(m_uMatWorld, 1, GL_FALSE, glm::value_ptr(matWorld));
		glUniformMatrix4fv(m_uMatVP, 1, GL_FALSE, glm::value_ptr(matVP));

		glEnable(GL_BLEND);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);

		// draw lines
		glUniform4fv(m_uColor, 1, glm::value_ptr(glm::vec4(1.0f, 1.0f, 1.0f, 0.5f)));
		glBindVertexArray(m_gridVAO);
		glDrawArrays(GL_LINES, 0, 160*2);

		// draw x
		glUniform4fv(m_uColor, 1, glm::value_ptr(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)));
		glBindVertexArray(m_xVAO);
		glDrawArrays(GL_LINES, 0, 2);

		// draw y
		glUniform4fv(m_uColor, 1, glm::value_ptr(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)));
		glBindVertexArray(m_yVAO);
		glDrawArrays(GL_LINES, 0, 2);

		// draw z
		glUniform4fv(m_uColor, 1, glm::value_ptr(glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)));
		glBindVertexArray(m_zVAO);
		glDrawArrays(GL_LINES, 0, 2);

		// draw points/vectors
		// TODO: instancing
		const std::vector<glm::vec4>& exprPositions = m_data->Debugger.GetVectorWatchPositions();
		const std::vector<glm::vec4>& exprColors = m_data->Debugger.GetVectorWatchColors();
		for (int i = 0; i < exprPositions.size(); i++) {
			const glm::vec4& pos = exprPositions[i];

			glUniform4fv(m_uColor, 1, glm::value_ptr(exprColors[i]));

			if (pos.w == 0.0f) {
				// vectors
				float length = glm::length(pos);
				glm::mat4 ypr = glm::mat4(1.0f);
				
				if (pos.x == 0.0f && pos.z == 0.0f)
					ypr = glm::rotate(glm::mat4(1.0f), -glm::sign(pos.y) * glm::pi<float>()/2.0f, glm::vec3(1, 0, 0));
				else
					ypr = glm::inverse(glm::lookAt(glm::vec3(0, 0, 0), -glm::vec3(pos), glm::vec3(0, 1, 0)));

				matWorld = ypr * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, glm::max<float>(0.0f, length - m_vectorPointSize)));
				glUniformMatrix4fv(m_uMatWorld, 1, GL_FALSE, glm::value_ptr(matWorld));
				m_vectorHandle.Draw();

				matWorld = ypr * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, length - 1 - m_vectorPointSize));
				glUniformMatrix4fv(m_uMatWorld, 1, GL_FALSE, glm::value_ptr(matWorld));
				m_vectorPoint.Draw();
			} else {
				// points
				matWorld = glm::translate(glm::mat4(1), glm::vec3(pos)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.075f));
				glUniformMatrix4fv(m_uMatWorld, 1, GL_FALSE, glm::value_ptr(matWorld));
				glBindVertexArray(m_unitSphereVAO);
				glDrawArrays(GL_TRIANGLES, 0, eng::GeometryFactory::VertexCount[ed::pipe::GeometryItem::GeometryType::Sphere]);
			}
		}

		matWorld = glm::mat4(1.0f);
		glUniformMatrix4fv(m_uMatWorld, 1, GL_FALSE, glm::value_ptr(matWorld));

		// draw unit sphere
		glUniform4fv(m_uColor, 1, glm::value_ptr(glm::vec4(1.0f, 1.0f, 1.0f, 0.25f)));
		glBindVertexArray(m_unitSphereVAO);
		glDrawArrays(GL_TRIANGLES, 0, eng::GeometryFactory::VertexCount[ed::pipe::GeometryItem::GeometryType::Sphere]);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		DefaultState::Bind();
	}
	void DebugVectorWatchUI::Refresh()
	{
		std::vector<char*>& exprs = m_data->Debugger.GetVectorWatchList();
		for (size_t i = 0; i < exprs.size(); i++)
			m_data->Debugger.UpdateVectorWatchValue(i);
	}
	void DebugVectorWatchUI::OnEvent(const SDL_Event& e)
	{
		if (e.type == SDL_MOUSEBUTTONDOWN)
			m_mouseContact = ImVec2(e.button.x, e.button.y);
	}
	void DebugVectorWatchUI::Update(float delta)
	{
		std::vector<char*>& exprs = m_data->Debugger.GetVectorWatchList();
		std::vector<glm::vec4>& clrs = m_data->Debugger.GetVectorWatchColors();

		float wWidth = ImGui::GetWindowContentRegionWidth();
		float wHeight = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
		float exprHeight = std::max<float>(250.0f, wHeight - 512 - ImGui::GetStyle().WindowPadding.y);
		float imgHeight = std::min<float>(512.0f, wHeight - exprHeight - ImGui::GetStyle().WindowPadding.y);
		float imgWidth = wWidth;

		float imgX = (wWidth - imgWidth) / 2.0f + ImGui::GetStyle().WindowPadding.x;

		if (imgWidth != m_lastFBOWidth || imgHeight != m_lastFBOHeight) {
			glBindTexture(GL_TEXTURE_2D, m_fboColor);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, imgWidth, imgHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glBindTexture(GL_TEXTURE_2D, m_fboDepth);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, imgWidth, imgHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);

			m_lastFBOWidth = imgWidth;
			m_lastFBOHeight = imgHeight;
		}

		m_render();

		// Main window
		ImGui::BeginChild("##vectorwatch_viewarea", ImVec2(0, exprHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

		
		if (ImGui::BeginTable("##vector_watch_tbl", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollFreezeTopRow | ImGuiTableFlags_ScrollY, ImVec2(0, Settings::Instance().CalculateSize(-ImGui::GetFontSize() - 10.0f)))) {
			ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
			ImGui::TableSetupColumn("Expression");
			ImGui::TableSetupColumn("Value");
			ImGui::TableAutoHeaders();

			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

			for (size_t i = 0; i < exprs.size(); i++) {
				ImGui::TableNextRow();

				ImGui::PushID(i);

				ImGui::TableSetColumnIndex(0);
				ImGui::ColorEdit4("##vectorwatch_color", const_cast<float*>(glm::value_ptr(clrs[i])), ImGuiColorEditFlags_NoInputs);

				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);
				if (ImGui::InputText("##vectorwatch_expr", exprs[i], 512, ImGuiInputTextFlags_EnterReturnsTrue)) {
					if (strlen(exprs[i]) == 0) {
						m_data->Debugger.RemoveVectorWatch(i);
						m_data->Parser.ModifyProject();
						i--;
						ImGui::PopID();
						continue;
					} else {
						m_data->Debugger.UpdateVectorWatchValue(i);
						m_data->Parser.ModifyProject();
					}
				}
				ImGui::PopItemWidth();

				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%s", m_data->Debugger.GetVectorWatchValue(i).c_str());

				ImGui::PopID();
			}

			ImGui::PopStyleColor();

			ImGui::EndTable();
		}

		ImGui::PushItemWidth(-1);
		if (ImGui::InputText("##vectorwatch_new_expr", m_newExpr, 512, ImGuiInputTextFlags_EnterReturnsTrue)) {
			glm::vec4 watchColor = glm::vec4(1.0f);
			unsigned char cr = 0, cg = 0, cb = 0;
			do {
				cr = rand() % 256;
				cg = rand() % 256;
				cb = rand() % 256;
			} while (cr < 50 && cg < 50 && cb < 50);
			watchColor = glm::vec4(cr / 255.0f, cg / 255.0f, cb / 255.0f, 1.0f);

			m_data->Debugger.AddVectorWatch(m_newExpr, watchColor);
			m_data->Parser.ModifyProject();
			m_newExpr[0] = 0;
		}
		ImGui::PopItemWidth();

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();

		ImGui::SetCursorPosX(imgX);
		ImGui::Image((ImTextureID)m_fboColor, ImVec2(imgWidth, imgHeight), ImVec2(0, 1), ImVec2(1, 0));
		if (ImGui::IsItemHovered()) {
			m_camera.Move(-ImGui::GetIO().MouseWheel);

			// handle right mouse dragging - camera
			if (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1)) {
				int ptX, ptY;
				SDL_GetMouseState(&ptX, &ptY);

				// get the delta from the last position
				int dX = ptX - m_mouseContact.x;
				int dY = ptY - m_mouseContact.y;

				// save the last position
				m_mouseContact = ImVec2(ptX, ptY);

				// rotate the camera according to the delta
				m_camera.Yaw(dX);
				m_camera.Pitch(dY);
			}
		}
	}
}