#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/Ray.h>
#include <SHADERed/Objects/DefaultState.h>
#include <SHADERed/Objects/GizmoObject.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/SystemVariableManager.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/intersect.hpp>

#include <iostream>

const char* GIZMO_VS_CODE = R"(
#version 330

layout (location = 0) in vec3 iPos;
layout (location = 1) in vec3 iNormal;

uniform mat4 uMatVP;
uniform mat4 uMatWorld;
uniform vec3 uColor;

out vec4 oColor;

const vec3 light1 = vec3(1,1,1);
const vec3 light2 = vec3(-1,1,-1);
const float lightRatio = 0.8;

void main() {
	vec3 normal = normalize(uMatWorld * vec4(iNormal, 0.0)).xyz;
	gl_Position = uMatVP * uMatWorld * vec4(iPos, 1.0f);
	
	vec3 lightVec1 = normalize(light1);
	vec3 lightVec2 = normalize(light2);
	
	float lightFactor = clamp(dot(lightVec1, normal), 0.0, 1.0) + clamp(dot(lightVec2, normal), 0.0, 1.0);
	vec3 clr = (1.0-lightRatio) * uColor + lightFactor*lightRatio*uColor;

	oColor = vec4(clr, 1.0);
}
)";

const char* GIZMO_PS_CODE = R"(
#version 330

in vec4 oColor;
out vec4 fragColor;

void main() {
	fragColor = oColor;
}
)";
const char* GUI_VS_CODE = R"(
#version 330
layout (location = 0) in vec3 iPos;

uniform mat4 uMatWVP;

void main() {
	gl_Position = uMatWVP * vec4(iPos, 1.0f);
}
)";

const char* GUI_PS_CODE = R"(
#version 330
out vec4 fragColor;

void main() {
	fragColor = vec4(1.0f,0.84313f,0.0f, 0.51f);
}
)";
#define GIZMO_SCALE_FACTOR 7.5f
#define GIZMO_HEIGHT 1.1f
#define GIZMO_WIDTH 0.05f
#define GIZMO_POINTER_WIDTH 0.1f
#define GIZMO_POINTER_HEIGHT 0.175f
#define GIZMO_PRECISE_COLBOX_WD 2.0f // increase the collision box by 100% in width and depth
#define GIZMO_SELECTED_COLOR glm::vec3(1, 0.84313f, 0)

#define GUI_POINT_COUNT 32
#define GUI_ROTA_RADIUS 115

namespace ed {
	GLuint CreateDegreeInfo(GLuint& vbo, float degreesStart, float degreesEnd)
	{
		int x = 0, y = 0;
		float radius = 115;

		const size_t count = GUI_POINT_COUNT * 3;

		float degrees = degreesEnd - degreesStart;
		if (degrees < 0) degrees += 360;

		float step = glm::radians(degrees) / GUI_POINT_COUNT;
		float radStart = glm::radians(degreesStart);

		const int numPoints = GUI_POINT_COUNT * 3;
		int numSegs = numPoints / 3;

		GLfloat circleData[numPoints * 8];

		for (int i = 0; i < numSegs; i++) {
			int j = i * 3 * 8;
			GLfloat* ptrData = &circleData[j];

			float xVal1 = sin(radStart + step * i);
			float yVal1 = cos(radStart + step * i);
			float xVal2 = sin(radStart + step * (i + 1));
			float yVal2 = cos(radStart + step * (i + 1));

			GLfloat point1[8] = { 0, 0, 0, 0, 0, 1, 0.5f, 0.5f };
			GLfloat point2[8] = { xVal1 * radius, yVal1 * radius, 0, 0, 0, 1, xVal1 * 0.5f + 0.5f, yVal1 * 0.5f + 0.5f };
			GLfloat point3[8] = { xVal2 * radius, yVal2 * radius, 0, 0, 0, 1, xVal2 * 0.5f + 0.5f, yVal2 * 0.5f + 0.5f };

			memcpy(ptrData + 0, point1, 8 * sizeof(GLfloat));
			memcpy(ptrData + 8, point2, 8 * sizeof(GLfloat));
			memcpy(ptrData + 16, point3, 8 * sizeof(GLfloat));
		}

		// create vao and vbo
		GLuint vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// create vbo
		glGenBuffers(1, &vbo);				// create buffer
		glBindBuffer(GL_ARRAY_BUFFER, vbo); // bind as vertex buffer

		// then copy the data to the bound vbo and unbind it
		glBufferData(GL_ARRAY_BUFFER, sizeof(circleData), circleData, GL_STATIC_DRAW);

		// configure input layout
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		// unbind
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		return vao;
	}

	GizmoObject::GizmoObject()
			: m_axisSelected(-1)
			, m_vw(-1)
			, m_vh(-1)
			, m_mode(0)
	{
		GLint success = false;
		char infoLog[512];

		ed::Logger::Get().Log("Initializing gizmo...");

		m_gizmoShader = gl::CreateShader(&GIZMO_VS_CODE, &GIZMO_PS_CODE, "gizmo");
		m_uiShader = gl::CreateShader(&GUI_VS_CODE, &GUI_PS_CODE, "GUI");

		// cache uniform locations
		m_uMatWorldLoc = glGetUniformLocation(m_gizmoShader, "uMatWorld");
		m_uMatVPLoc = glGetUniformLocation(m_gizmoShader, "uMatVP");
		m_uColorLoc = glGetUniformLocation(m_gizmoShader, "uColor");
		m_uMatWVPLoc = glGetUniformLocation(m_uiShader, "uMatWVP");

		// gizmo and ui
		m_uiVAO = CreateDegreeInfo(m_uiVBO, 45, 90);

		bool gizmo3DLoaded = m_model.LoadFromFile("data/gizmo.obj");

		if (gizmo3DLoaded)
			ed::Logger::Get().Log("Loaded gizmo 3D model");
		else
			ed::Logger::Get().Log("Failed to load gizmo 3D model", true);

		m_axisHovered = -1;
		m_colors[0] = glm::vec3(1, 0, 0);
		m_colors[1] = glm::vec3(0, 1, 0);
		m_colors[2] = glm::vec3(0, 0, 1);

		ed::Logger::Get().Log("Finished with initializing gizmo");
	}
	GizmoObject::~GizmoObject()
	{
		glDeleteProgram(m_gizmoShader);
		glDeleteProgram(m_uiShader);
		glDeleteBuffers(1, &m_uiVBO);
		glDeleteVertexArrays(1, &m_uiVAO);
	}
	void GizmoObject::HandleMouseMove(int x, int y, int vw, int vh)
	{
		// handle dragging rotation controls
		if (m_axisSelected != -1 && m_mode == 2) {
			// rotate the object
			float degNow = glm::degrees(atan2(x - (vw / 2), y - (vh / 2)));
			float deg = degNow - m_clickDegrees;

			if (Settings::Instance().Preview.GizmoRotationUI) {
				glDeleteVertexArrays(1, &m_uiVAO);
				glDeleteBuffers(1, &m_uiVBO);
				m_uiVAO = CreateDegreeInfo(m_uiVBO, m_clickDegrees, degNow);
			}

			if (deg < 0) deg = 360 + deg;

			int snap = Settings::Instance().Preview.GizmoSnapRotation;
			if (snap > 0)
				m_tValue.x = (int)(deg / snap) * snap;
			else
				m_tValue.x = deg;

			float rad = m_tValue.x / 180 * glm::pi<float>();
			switch (m_axisSelected) {
			case 0:
				m_rota->x = m_curValue.x + rad;
				if (m_rota->x >= 2 * glm::pi<float>())
					m_rota->x -= (int)(m_rota->x / (2 * glm::pi<float>())) * glm::pi<float>() * 2;
				break;
			case 1:
				m_rota->y = m_curValue.y + rad;
				if (m_rota->y >= 2 * glm::pi<float>())
					m_rota->y -= (int)(m_rota->y / (2 * glm::pi<float>())) * 2 * glm::pi<float>();
				break;
			case 2:
				m_rota->z = m_curValue.z + rad;
				if (m_rota->z >= 2 * glm::pi<float>())
					m_rota->z -= (int)(m_rota->z / (2 * glm::pi<float>())) * 2 * glm::pi<float>();
				break;
			}
		}

		// handle hovering over controls
		m_axisHovered = -1;
		m_vw = vw;
		m_vh = vh;

		float scale = glm::length(*m_trans - glm::vec3(SystemVariableManager::Instance().GetCamera()->GetPosition())) / GIZMO_SCALE_FACTOR;
		if (scale == 0.0f)
			return;

		// selection
		// X axis
		glm::mat4 xWorld = glm::translate(glm::mat4(1), *m_trans)
			* glm::rotate(glm::mat4(1), -glm::half_pi<float>(), glm::vec3(0, 0, 1));

		// Y axis
		glm::mat4 yWorld = glm::translate(glm::mat4(1), *m_trans);

		// Z axis
		glm::mat4 zWorld = glm::translate(glm::mat4(1), *m_trans) * glm::rotate(glm::mat4(1), glm::half_pi<float>(), glm::vec3(1, 0, 0));

		float mouseX = x / (vw * 0.5f) - 1.0f;
		float mouseY = y / (vh * 0.5f) - 1.0f;

		glm::mat4 proj = SystemVariableManager::Instance().GetProjectionMatrix();
		glm::mat4 view = SystemVariableManager::Instance().GetCamera()->GetMatrix();

		glm::mat4 invVP = glm::inverse(proj * view);
		glm::vec4 screenPos(mouseX, mouseY, 1.0f, 1.0f);
		glm::vec4 worldPos = invVP * screenPos;

		glm::vec3 rayDir = glm::normalize(glm::vec3(worldPos));
		glm::vec3 rayOrigin = glm::vec3(SystemVariableManager::Instance().GetCamera()->GetPosition());

		if (m_mode == 0 || m_mode == 1) {
			float depth = std::numeric_limits<float>::infinity();

			m_axisHovered = m_getBasicAxisSelection(x, y, vw, vh, depth);

			if (m_axisHovered != -1) {
				m_hoverDepth = m_lastDepth = depth;
				m_hoverStart = rayOrigin + m_hoverDepth * rayDir;
			}
		} else { // handle the rotation bars
			glm::mat4 matWorld = glm::translate(glm::mat4(1), *m_trans) * glm::rotate(glm::mat4(1), glm::half_pi<float>(), glm::vec3(1, 0, 0)) * glm::scale(glm::mat4(1), glm::vec3(scale));

			float triDist = 0;
			float curDist = std::numeric_limits<float>::infinity();
			int selIndex = -1;

			std::vector<std::string> models = { "RotateX", "RotateY", "RotateZ" };

			for (auto& mesh : m_model.Meshes) {
				for (int j = 0; j < models.size(); j++) {
					if (mesh.Name != models[j])
						continue;

					for (int i = 0; i < mesh.Vertices.size(); i += 3) {
						glm::vec3 v0 = matWorld * glm::vec4(mesh.Vertices[i + 0].Position, 1);
						glm::vec3 v1 = matWorld * glm::vec4(mesh.Vertices[i + 1].Position, 1);
						glm::vec3 v2 = matWorld * glm::vec4(mesh.Vertices[i + 2].Position, 1);

						if (ray::IntersectTriangle(rayOrigin, rayDir, v0, v1, v2, triDist)) {
							if (triDist < curDist) {
								curDist = triDist;
								selIndex = j;
							}
						}
					}
				}
			}

			if (selIndex != -1) {
				m_axisHovered = selIndex;
				m_hoverDepth = curDist;
				m_hoverStart = rayOrigin + rayDir * m_hoverDepth;
			}
		}
	}
	int GizmoObject::Click(int x, int y, int vw, int vh)
	{
		// update the hover data
		HandleMouseMove(x, y, vw, vh);
		// handle control selection
		m_axisSelected = m_axisHovered;
		if (m_axisSelected != -1) {
			m_clickDepth = m_hoverDepth;
			m_clickStart = m_hoverStart;

			if (m_mode == 2) {
				m_clickDegrees = glm::degrees(atan2(x - (vw / 2), y - (vh / 2)));

				if (Settings::Instance().Preview.GizmoRotationUI) {
					glDeleteVertexArrays(1, &m_uiVAO);
					glDeleteBuffers(1, &m_uiVBO);
					m_uiVAO = CreateDegreeInfo(m_uiVBO, 0, 0);
				}
			}

			switch (m_mode) {
			case 0: m_curValue = *m_trans; break;
			case 1: m_curValue = *m_scale; break;
			case 2: m_curValue = *m_rota; break;
			}
			m_tValue = glm::vec3(0, 0, 0);
		}

		return m_axisSelected;
	}
	bool GizmoObject::Transform(int x, int y, bool shift)
	{
		// dont handle the rotation controls here as we handle that in the HandleMouseMove method
		if (m_axisSelected == -1 || m_mode == 2)
			return false;

		bool ret = true;

		// handle scaling and translating
		float mouseX = x / (m_vw * 0.5f) - 1.0f;
		float mouseY = y / (m_vh * 0.5f) - 1.0f;

		glm::mat4 proj = SystemVariableManager::Instance().GetProjectionMatrix();
		glm::mat4 view = SystemVariableManager::Instance().GetCamera()->GetMatrix();

		glm::mat4 invVP = glm::inverse(proj * view);
		glm::vec4 screenPos(mouseX, mouseY, 1.0f, 1.0f);
		glm::vec4 worldPos = invVP * screenPos;

		glm::vec3 rayDir = glm::normalize(glm::vec3(worldPos));
		glm::vec4 rayOrigin = SystemVariableManager::Instance().GetCamera()->GetPosition();

		bool normalView = glm::abs(glm::dot(rayDir, glm::vec3(0, 1, 0))) < 0.7f;

		glm::vec3 axisVec(m_axisSelected == 0, m_axisSelected == 1, m_axisSelected == 2);
		glm::vec3 planeOrigin = *m_trans;
		glm::vec4 planeNormal = glm::vec4(glm::normalize(planeOrigin - glm::vec3(SystemVariableManager::Instance().GetCamera()->GetPosition())), 0);
		
		float depth = std::numeric_limits<float>::infinity();
		bool rpInters = glm::intersectRayPlane(rayOrigin, glm::vec4(rayDir, 0), glm::vec4(planeOrigin, 1), planeNormal, depth);
		
		if (!rpInters || depth == std::numeric_limits<float>::infinity())
			return false;

		glm::vec4 mouseVec = rayOrigin + depth * glm::vec4(rayDir, 0.0f);
		glm::vec3 moveVec = glm::vec3(glm::vec3(mouseVec) - m_clickStart);

		if (m_mode == 0) {
			m_tValue += moveVec * glm::vec3(axisVec);

			int snap = Settings::Instance().Preview.GizmoSnapTranslation;
			if (snap <= 0)
				*m_trans = m_curValue + m_tValue;
			else
				*m_trans = m_curValue + glm::vec3((glm::ivec3(m_tValue) / snap) * snap);

			ret = true;
		} else if (m_mode == 1) {
			m_tValue += moveVec * glm::vec3(axisVec);

			int snap = Settings::Instance().Preview.GizmoSnapScale;
			if (snap <= 0)
				*m_scale = m_curValue + m_tValue;
			else
				*m_scale = m_curValue + glm::vec3((glm::ivec3(m_tValue) / snap) * snap);

			ret = true;
		}

		m_clickStart = mouseVec;

		return ret;
	}
	void GizmoObject::Render()
	{
		float scale = glm::length(*m_trans - glm::vec3(SystemVariableManager::Instance().GetCamera()->GetPosition())) / GIZMO_SCALE_FACTOR;

		glUseProgram(m_gizmoShader);

		glm::mat4 matWorld = glm::translate(glm::mat4(1), *m_trans) * glm::rotate(glm::mat4(1), -glm::half_pi<float>(), glm::vec3(1, 0, 0)) * glm::scale(glm::mat4(1), glm::vec3(scale));

		glUniformMatrix4fv(m_uMatWorldLoc, 1, GL_FALSE, glm::value_ptr(matWorld));
		glUniformMatrix4fv(m_uMatVPLoc, 1, GL_FALSE, glm::value_ptr(m_proj * m_view));

		std::vector<std::string> objects;
		if (m_mode == 0) // translation
			objects = { "HandleX", "ArrowX", "HandleY", "ArrowY", "HandleZ", "ArrowZ" };
		else if (m_mode == 1) // Scale
			objects = { "HandleX", "CyX", "HandleY", "CyY", "HandleZ", "CyZ" };
		else if (m_mode == 2) // Rotation
			objects = { "RotateX", "RotateY", "RotateZ" };

		for (auto& obj : objects) {
			int id = 0;
			if (obj.find("Y") != std::string::npos)
				id = 1;
			else if (obj.find("Z") != std::string::npos)
				id = 2;
			glm::vec3 clr = (id == m_axisHovered) ? GIZMO_SELECTED_COLOR : m_colors[id];

			glUniform3fv(m_uColorLoc, 1, glm::value_ptr(clr));

			m_model.Draw(obj);
		}

		// degree info UI
		if (m_axisSelected != -1 && m_mode == 2 && Settings::Instance().Preview.GizmoRotationUI) {
			glUseProgram(m_uiShader);

			glm::mat4 matProj = glm::ortho(0.0f, m_vw, m_vh, 0.0f, 0.1f, 1000.0f);
			glm::mat4 uMatWVP = matProj * glm::translate(glm::mat4(1), glm::vec3(m_vw / 2, m_vh / 2, -1)) * glm::rotate(glm::mat4(1), glm::pi<float>(), glm::vec3(1, 0, 0));

			glUniformMatrix4fv(m_uMatWVPLoc, 1, GL_FALSE, glm::value_ptr(uMatWVP));

			glDisable(GL_DEPTH_CLAMP);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
			glBlendEquationSeparate(GL_ADD, GL_MAX);

			glBindVertexArray(m_uiVAO);
			glDrawArrays(GL_TRIANGLES, 0, GUI_POINT_COUNT * 3);

			DefaultState::Bind();
		}
	}
	int GizmoObject::m_getBasicAxisSelection(int mx, int my, int vw, int vh, float& depth)
	{
		float scale = glm::length(*m_trans - glm::vec3(SystemVariableManager::Instance().GetCamera()->GetPosition())) / GIZMO_SCALE_FACTOR;
		if (scale == 0.0f)
			return -1;

		// selection
		// X axis
		glm::mat4 xWorld = glm::translate(glm::mat4(1), *m_trans)
			* glm::rotate(glm::mat4(1), -glm::half_pi<float>(), glm::vec3(0, 0, 1));

		// Y axis
		glm::mat4 yWorld = glm::translate(glm::mat4(1), *m_trans);

		// Z axis
		glm::mat4 zWorld = glm::translate(glm::mat4(1), *m_trans) * glm::rotate(glm::mat4(1), glm::half_pi<float>(), glm::vec3(1, 0, 0));

		float mouseX = mx / (vw * 0.5f) - 1.0f;
		float mouseY = my / (vh * 0.5f) - 1.0f;

		glm::mat4 proj = SystemVariableManager::Instance().GetProjectionMatrix();
		glm::mat4 view = SystemVariableManager::Instance().GetCamera()->GetMatrix();

		glm::mat4 invVP = glm::inverse(proj * view);
		glm::vec4 screenPos(mouseX, mouseY, 1.0f, 1.0f);
		glm::vec4 worldPos = invVP * screenPos;

		glm::vec3 rayDir = glm::normalize(glm::vec3(worldPos));
		glm::vec3 rayOrigin = glm::vec3(SystemVariableManager::Instance().GetCamera()->GetPosition());

		int axisRet = -1;
		if (m_mode == 0 || m_mode == 1) {
			glm::vec3 maxP(GIZMO_WIDTH * GIZMO_PRECISE_COLBOX_WD * scale / 2, (GIZMO_HEIGHT + GIZMO_POINTER_HEIGHT) * scale, GIZMO_WIDTH * GIZMO_PRECISE_COLBOX_WD * scale / 2);
			glm::vec3 minP(-maxP.x, 0, -maxP.z);

			// X axis
			glm::vec3 b1 = xWorld * glm::vec4(minP, 1);
			glm::vec3 b2 = xWorld * glm::vec4(maxP, 1);

			float distX, distY, distZ, dist = std::numeric_limits<float>::infinity();
			if (ray::IntersectBox(rayOrigin, rayDir, b1, b2, distX))
				axisRet = 0, dist = distX;
			else
				distX = std::numeric_limits<float>::infinity();

			// Y axis
			b1 = yWorld * glm::vec4(minP, 1);
			b2 = yWorld * glm::vec4(maxP, 1);

			if (ray::IntersectBox(rayOrigin, rayDir, b1, b2, distY) && distY < distX)
				axisRet = 1, dist = distY;
			else
				distY = std::numeric_limits<float>::infinity();

			// Z axis
			b1 = zWorld * glm::vec4(minP, 1);
			b2 = zWorld * glm::vec4(maxP, 1);

			if (ray::IntersectBox(rayOrigin, rayDir, b1, b2, distZ) && distZ < distY && distZ < distX)
				axisRet = 2, dist = distZ;
			else
				distZ = std::numeric_limits<float>::infinity();

			if (axisRet != -1) {
				glm::vec3 planeOrigin = *m_trans;
				glm::vec4 planeNormal = glm::vec4(glm::normalize(planeOrigin - glm::vec3(SystemVariableManager::Instance().GetCamera()->GetPosition())), 0);
				glm::intersectRayPlane(glm::vec4(rayOrigin, 1), glm::vec4(rayDir, 0), glm::vec4(planeOrigin, 1), planeNormal, depth);
			}
		}

		return axisRet;
	}
}