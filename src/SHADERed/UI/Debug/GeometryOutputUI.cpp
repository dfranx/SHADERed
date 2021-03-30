#include <SHADERed/UI/Debug/GeometryOutputUI.h>
#include <SHADERed/Objects/DefaultState.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/Engine/GLUtils.h>
#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

const char* GEOMETRY_OUTPUT_VS_CODE = R"(
#version 330

layout (location = 0) in vec4 iPos;

uniform mat4 uMat;

void main() {
	gl_Position = uMat * iPos;
}
)";

const char* GEOMETRY_OUTPUT_PS_CODE = R"(
#version 330

out vec4 fragColor;

void main() {
	fragColor = vec4(0.329f, 0.6f, 0.780f, 1.0f);
}
)";


namespace ed {
	DebugGeometryOutputUI::DebugGeometryOutputUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name, bool visible)
			: UIView(ui, objects, name, visible)
	{
		m_bufLinesVAO = 0;
		m_bufPointsVAO = 0;
		m_bufTrianglesVAO = 0;
		m_bufLinesVBO = 0;
		m_bufPointsVBO = 0;
		m_bufTrianglesVBO = 0;

		m_is3D = 0;

		m_fbo = m_fboColor = m_fboDepth = 0;
		m_lastFBOWidth = m_lastFBOHeight = 512.0f;
		m_fbo = gl::CreateSimpleFramebuffer(512, 512, m_fboColor, m_fboDepth, GL_RGB);

		m_lines = m_points = m_triangles = 0;

		m_shader = gl::CreateShader(&GEOMETRY_OUTPUT_VS_CODE, &GEOMETRY_OUTPUT_PS_CODE, "vector watch");

		m_uMatLoc = glGetUniformLocation(m_shader, "uMat");
	}
	DebugGeometryOutputUI::~DebugGeometryOutputUI()
	{
		glDeleteBuffers(1, &m_bufPointsVBO);
		glDeleteVertexArrays(1, &m_bufPointsVAO);
		glDeleteBuffers(1, &m_bufLinesVBO);
		glDeleteVertexArrays(1, &m_bufLinesVAO);
		glDeleteBuffers(1, &m_bufTrianglesVBO);
		glDeleteVertexArrays(1, &m_bufTrianglesVAO);

		glDeleteTextures(1, &m_fboColor);
		glDeleteTextures(1, &m_fboDepth);
		glDeleteFramebuffers(1, &m_fbo);

		glDeleteShader(m_shader);
	}

	void DebugGeometryOutputUI::m_buildVertexBuffers()
	{
		glDeleteBuffers(1, &m_bufPointsVBO);
		glDeleteVertexArrays(1, &m_bufPointsVAO);
		glDeleteBuffers(1, &m_bufLinesVBO);
		glDeleteVertexArrays(1, &m_bufLinesVAO);
		glDeleteBuffers(1, &m_bufTrianglesVBO);
		glDeleteVertexArrays(1, &m_bufTrianglesVAO);
		m_lines = m_points = m_triangles = 0;
		m_indexPos.clear();

		// build data lists
		std::vector<float> linesData, trianglesData, pointsData;
		PixelInformation* px = m_data->Debugger.GetPixel();
		if (px) {
			// triangle data
			if (px->GeometryOutputType == GeometryShaderOutput::TriangleStrip) {
				for (auto& prim : px->GeometryOutput) {
					if (prim.Position.size() >= 3) {
						for (int v = 2; v < prim.Position.size(); v++) {
							// every other triangle is CW, so switch it up
							int d1 = 2, d2 = 1;
							if (v % 2 == 1) {
								d1 = 1;
								d2 = 2;
							}

							trianglesData.push_back(prim.Position[v - d1].x);
							trianglesData.push_back(prim.Position[v - d1].y);
							trianglesData.push_back(prim.Position[v - d1].z);
							trianglesData.push_back(prim.Position[v - d1].w);

							trianglesData.push_back(prim.Position[v - d2].x);
							trianglesData.push_back(prim.Position[v - d2].y);
							trianglesData.push_back(prim.Position[v - d2].z);
							trianglesData.push_back(prim.Position[v - d2].w);

							trianglesData.push_back(prim.Position[v].x);
							trianglesData.push_back(prim.Position[v].y);
							trianglesData.push_back(prim.Position[v].z);
							trianglesData.push_back(prim.Position[v].w);

							m_triangles += 3;
						}
					}
				}
			}

			// line data
			if (px->GeometryOutputType == GeometryShaderOutput::LineStrip) {
				for (auto& prim : px->GeometryOutput) {
					if (prim.Position.size() >= 2) {
						for (int v = 1; v < prim.Position.size(); v++) {
							linesData.push_back(prim.Position[v - 1].x);
							linesData.push_back(prim.Position[v - 1].y);
							linesData.push_back(prim.Position[v - 1].z);
							linesData.push_back(prim.Position[v - 1].w);

							linesData.push_back(prim.Position[v].x);
							linesData.push_back(prim.Position[v].y);
							linesData.push_back(prim.Position[v].z);
							linesData.push_back(prim.Position[v].w);

							m_lines += 2;
						}
					}
				}
			}
			else if (px->GeometryOutputType == GeometryShaderOutput::TriangleStrip) {
				for (auto& prim : px->GeometryOutput) {
					if (prim.Position.size() == 2) {
						linesData.push_back(prim.Position[0].x);
						linesData.push_back(prim.Position[0].y);
						linesData.push_back(prim.Position[0].z);
						linesData.push_back(prim.Position[0].w);

						linesData.push_back(prim.Position[1].x);
						linesData.push_back(prim.Position[1].y);
						linesData.push_back(prim.Position[1].z);
						linesData.push_back(prim.Position[1].w);

						m_lines += 2;
					}
				}
			}

			// point data
			if (px->GeometryOutputType == GeometryShaderOutput::Points) {
				for (auto& prim : px->GeometryOutput) {
					for (int v = 0; v < prim.Position.size(); v++) {
						pointsData.push_back(prim.Position[v].x);
						pointsData.push_back(prim.Position[v].y);
						pointsData.push_back(prim.Position[v].z);
						pointsData.push_back(prim.Position[v].w);

						m_points++;
					}
				}
			} 
			else if (px->GeometryOutputType == GeometryShaderOutput::LineStrip) {
				for (auto& prim : px->GeometryOutput) {
					if (prim.Position.size() == 1) {
						pointsData.push_back(prim.Position[0].x);
						pointsData.push_back(prim.Position[0].y);
						pointsData.push_back(prim.Position[0].z);
						pointsData.push_back(prim.Position[0].w);

						m_points++;
					}
				}
			}
			else if (px->GeometryOutputType == GeometryShaderOutput::TriangleStrip) {
				for (auto& prim : px->GeometryOutput) {
					if (prim.Position.size() == 1) {
						pointsData.push_back(prim.Position[0].x);
						pointsData.push_back(prim.Position[0].y);
						pointsData.push_back(prim.Position[0].z);
						pointsData.push_back(prim.Position[0].w);

						m_points++;
					}
				}
			}
		
			// index data
			for (auto& prim : px->GeometryOutput)
				for (int v = 0; v < prim.Position.size(); v++) {
					m_indexPos.push_back(ImVec2(0.0f, 0.0f));
					m_indexPos.back().x = (prim.Position[v].x / prim.Position[v].w + 1.0f) * 0.5f;
					m_indexPos.back().y = 1.0f - (prim.Position[v].y / prim.Position[v].w + 1.0f) * 0.5f;
				}
		}

		if (m_is3D) {
			glm::mat4 mat = glm::inverse(SystemVariableManager::Instance().GetViewProjectionMatrix());

			for (int i = 0; i < trianglesData.size(); i += 4) {
				glm::vec4 res = mat * glm::make_vec4(trianglesData.data() + i);
				trianglesData[i + 0] = res[0];
				trianglesData[i + 1] = res[1];
				trianglesData[i + 2] = res[2];
				trianglesData[i + 3] = res[3];
			}
			for (int i = 0; i < linesData.size(); i += 4) {
				glm::vec4 res = mat * glm::make_vec4(linesData.data() + i);
				linesData[i + 0] = res[0];
				linesData[i + 1] = res[1];
				linesData[i + 2] = res[2];
				linesData[i + 3] = res[3];
			}
			for (int i = 0; i < pointsData.size(); i += 4) {
				glm::vec4 res = mat * glm::make_vec4(pointsData.data() + i);
				pointsData[i + 0] = res[0];
				pointsData[i + 1] = res[1];
				pointsData[i + 2] = res[2];
				pointsData[i + 3] = res[3];
			}
		} else {
			// normalize coordinates
			for (int i = 0; i < linesData.size(); i += 4) {
				linesData[i + 0] /= linesData[i + 3];
				linesData[i + 1] /= linesData[i + 3];
				linesData[i + 2] /= linesData[i + 3];
				linesData[i + 3] = 1.0f;
			}
			for (int i = 0; i < pointsData.size(); i += 4) {
				pointsData[i + 0] /= pointsData[i + 3];
				pointsData[i + 1] /= pointsData[i + 3];
				pointsData[i + 2] /= pointsData[i + 3];
				pointsData[i + 3] = 1.0f;
			}
			for (int i = 0; i < trianglesData.size(); i += 4) {
				trianglesData[i + 0] /= trianglesData[i + 3];
				trianglesData[i + 1] /= trianglesData[i + 3];
				trianglesData[i + 2] /= trianglesData[i + 3];
				trianglesData[i + 3] = 1.0f;
			}
		}

		// lines vbo
		glGenBuffers(1, &m_bufLinesVBO);
		glBindBuffer(GL_ARRAY_BUFFER, m_bufLinesVBO);
		glBufferData(GL_ARRAY_BUFFER, linesData.size() * sizeof(GLfloat), linesData.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// lines vao
		glDeleteVertexArrays(1, &m_bufLinesVAO);
		glGenVertexArrays(1, &m_bufLinesVAO);
		glBindVertexArray(m_bufLinesVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_bufLinesVBO);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// triangles vbo
		glGenBuffers(1, &m_bufTrianglesVBO);
		glBindBuffer(GL_ARRAY_BUFFER, m_bufTrianglesVBO);
		glBufferData(GL_ARRAY_BUFFER, trianglesData.size() * sizeof(GLfloat), trianglesData.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// triangles vao
		glDeleteVertexArrays(1, &m_bufTrianglesVAO);
		glGenVertexArrays(1, &m_bufTrianglesVAO);
		glBindVertexArray(m_bufTrianglesVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_bufTrianglesVBO);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// points vbo
		glGenBuffers(1, &m_bufPointsVBO);
		glBindBuffer(GL_ARRAY_BUFFER, m_bufPointsVBO);
		glBufferData(GL_ARRAY_BUFFER, pointsData.size() * sizeof(GLfloat), pointsData.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// points vao
		glDeleteVertexArrays(1, &m_bufPointsVAO);
		glGenVertexArrays(1, &m_bufPointsVAO);
		glBindVertexArray(m_bufPointsVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_bufPointsVBO);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	void DebugGeometryOutputUI::m_render()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		static const GLuint fboBuffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, fboBuffers);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
		glClearBufferfv(GL_COLOR, 0, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
		glViewport(0, 0, m_lastFBOWidth, m_lastFBOHeight);

		glUseProgram(m_shader);

		if (m_is3D) {
			glm::mat4 matVP = glm::perspective(glm::radians(45.0f), m_lastFBOWidth / m_lastFBOHeight, 0.1f, 1000.0f) * m_camera.GetMatrix();
			glUniformMatrix4fv(m_uMatLoc, 1, GL_FALSE, glm::value_ptr(matVP));
		} else
			glUniformMatrix4fv(m_uMatLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

		glBindVertexArray(m_bufLinesVAO);
		glDrawArrays(GL_LINES, 0, m_lines);

		glBindVertexArray(m_bufTrianglesVAO);
		glDrawArrays(GL_TRIANGLES, 0, m_triangles);

		glBindVertexArray(m_bufPointsVAO);
		glDrawArrays(GL_POINTS, 0, m_points);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		DefaultState::Bind();
	}
	void DebugGeometryOutputUI::OnEvent(const SDL_Event& e)
	{
		if (e.type == SDL_MOUSEBUTTONDOWN)
			m_mouseContact = ImVec2(e.button.x, e.button.y);
	}
	void DebugGeometryOutputUI::Update(float delta)
	{
		// update VBOs
		if (m_data->Debugger.IsGeometryUpdated()) {
			m_buildVertexBuffers();
			m_data->Debugger.ResetGeometryUpdated();
		}

		// combo
		ImGui::PushItemWidth(-1);
		if (ImGui::Combo("##geoout_3d", &m_is3D, " NDC\0 3D\0"))
			m_buildVertexBuffers();
		ImGui::PopItemWidth();

		// update FBO
		float wndWidth = ImGui::GetWindowContentRegionWidth();
		float wndHeight = ImGui::GetContentRegionAvail().y;
		if (m_lastFBOHeight != wndHeight || m_lastFBOWidth != wndWidth) {
			glBindTexture(GL_TEXTURE_2D, m_fboColor);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, wndWidth, wndHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glBindTexture(GL_TEXTURE_2D, m_fboDepth);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, wndWidth, wndHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);

			m_lastFBOWidth = wndWidth;
			m_lastFBOHeight = wndHeight;
		}

		// draw the render texture contents but darkened
		PixelInformation* px = m_data->Debugger.GetPixel();
		if (px != nullptr && m_is3D == 0) {
			ImVec2 curPos = ImGui::GetCursorPos();

			if (px->RenderTexture != nullptr)
				ImGui::Image((ImTextureID)px->RenderTexture->Texture, ImVec2(m_lastFBOWidth, m_lastFBOHeight), ImVec2(0, 1), ImVec2(1, 0), ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			else
				ImGui::Image((ImTextureID)m_data->Renderer.GetTexture(), ImVec2(m_lastFBOWidth, m_lastFBOHeight), ImVec2(0, 1), ImVec2(1, 0), ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		
			ImGui::SetCursorPos(curPos);
		}
		// render the contents
		m_render();

		// draw the contents on the window
		ImVec2 uiPos = ImGui::GetCursorScreenPos();
		ImGui::Image((ImTextureID)m_fboColor, ImVec2(m_lastFBOWidth, m_lastFBOHeight), ImVec2(0, 1), ImVec2(1, 0), ImVec4(0.9f, 0.9f, 0.9f, 0.6f));

		// camera controlls
		if (ImGui::IsItemHovered()) {
			m_camera.Move(-ImGui::GetIO().MouseWheel);

			// handle right mouse dragging - camera
			if (ImGui::IsMouseDown(1)) {
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

		// draw indices
		if (m_is3D == 0) {
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			for (int i = 0; i < m_indexPos.size(); i++)
				drawList->AddText(ImVec2(uiPos.x + m_indexPos[i].x * wndWidth, uiPos.y + m_indexPos[i].y * wndHeight), 0xFFFFFFFF, std::to_string(i).c_str());
		}
	}
}