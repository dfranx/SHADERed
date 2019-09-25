#include "GizmoObject.h"
#include "Logger.h"
#include "DefaultState.h"
#include "SystemVariableManager.h"
#include "../Engine/GeometryFactory.h"
#include "../Engine/Ray.h"

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
#define GIZMO_SELECTED_COLOR glm::vec3(1,0.84313f,0)

#define GUI_POINT_COUNT 32
#define GUI_ROTA_RADIUS 115

namespace ed
{
	GLuint CreateDegreeInfo(GLuint& vbo, float degreesStart, float degreesEnd)
	{
		int x = 0, y = 0;
		float radius = 115;

		const size_t count = GUI_POINT_COUNT*3;

		float degrees = degreesEnd - degreesStart;
		if (degrees < 0) degrees += 360;

		float step = glm::radians(degrees) / GUI_POINT_COUNT;
		float radStart = glm::radians(degreesStart);


		const int numPoints = GUI_POINT_COUNT * 3;
		int numSegs = numPoints / 3;

		GLfloat circleData[numPoints * 8];


		for (int i = 0; i < numSegs; i++)
		{
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
		glBindBuffer(GL_ARRAY_BUFFER, vbo);	// bind as vertex buffer

		// then copy the data to the bound vbo and unbind it
		glBufferData(GL_ARRAY_BUFFER, sizeof(circleData), circleData, GL_STATIC_DRAW);
	
		// configure input layout
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
	
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3* sizeof(float)));
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6* sizeof(float)));
		glEnableVertexAttribArray(2);

		// unbind
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		return vao;
	}

	GizmoObject::GizmoObject() :
		m_axisSelected(-1), m_vw(-1), m_vh(-1),
		m_mode(0)
	{
		GLint success = false;
		char infoLog[512];

		ed::Logger::Get().Log("Initializing gizmo...", false, __FILE__, __LINE__);
		ed::Logger::Get().Log("Loading shaders...");
		
		// create vertex shader
		unsigned int gizmoVS = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(gizmoVS, 1, &GIZMO_VS_CODE, nullptr);
		glCompileShader(gizmoVS);
		glGetShaderiv(gizmoVS, GL_COMPILE_STATUS, &success);
		if(!success) {
			glGetShaderInfoLog(gizmoVS, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile a gizmo vertex shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create pixel shader
		unsigned int gizmoPS = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(gizmoPS, 1, &GIZMO_PS_CODE, nullptr);
		glCompileShader(gizmoPS);
		glGetShaderiv(gizmoPS, GL_COMPILE_STATUS, &success);
		if(!success) {
			glGetShaderInfoLog(gizmoPS, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile a gizmo pixel shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create vertex shader
		unsigned int uiVS = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(uiVS, 1, &GUI_VS_CODE, nullptr);
		glCompileShader(uiVS);
		glGetShaderiv(uiVS, GL_COMPILE_STATUS, &success);
		if(!success) {
			glGetShaderInfoLog(uiVS, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile a GUI vertex shader", true, __FILE__, __LINE__);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create pixel shader
		unsigned int uiPS = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(uiPS, 1, &GUI_PS_CODE, nullptr);
		glCompileShader(uiPS);
		glGetShaderiv(uiPS, GL_COMPILE_STATUS, &success);
		if(!success) {
			glGetShaderInfoLog(uiPS, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile a GUI pixel shader", true, __FILE__, __LINE__);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create a shader program for gizmo
		m_gizmoShader = glCreateProgram();
		glAttachShader(m_gizmoShader, gizmoVS);
		glAttachShader(m_gizmoShader, gizmoPS);
		glLinkProgram(m_gizmoShader);
		glGetProgramiv(m_gizmoShader, GL_LINK_STATUS, &success);
		if(!success) {
			glGetProgramInfoLog(m_gizmoShader, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to create a gizmo shader program", true);
			ed::Logger::Get().Log(infoLog, true);
		}
		

		// create a shader program for gui
		m_uiShader = glCreateProgram();
		glAttachShader(m_uiShader, uiVS);
		glAttachShader(m_uiShader, uiPS);
		glLinkProgram(m_uiShader);
		glGetProgramiv(m_uiShader, GL_LINK_STATUS, &success);
		if(!success) {
			glGetProgramInfoLog(m_uiShader, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to create a GUI shader program", true, __FILE__, __LINE__);
			ed::Logger::Get().Log(infoLog, true);
		}

		glDeleteShader(gizmoVS);
		glDeleteShader(gizmoPS);
		glDeleteShader(uiVS);
		glDeleteShader(uiPS);

		// cache uniform locations
		m_uMatWorldLoc = glGetUniformLocation(m_gizmoShader, "uMatWorld");
		m_uMatVPLoc = glGetUniformLocation(m_gizmoShader, "uMatVP");
		m_uColorLoc = glGetUniformLocation(m_gizmoShader, "uColor");
		m_uMatWVPLoc = glGetUniformLocation(m_uiShader, "uMatWVP");
		
		// gizmo and ui
		m_uiVAO = CreateDegreeInfo(m_uiVBO, 45, 90);

		ed::Logger::Get().Log("Loading gizmo 3D model...");
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
			float deg = degNow-m_clickDegrees;

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
				if (m_rota->x >= 2*glm::pi<float>())
					m_rota->x -= (int)(m_rota->x / (2*glm::pi<float>())) * glm::pi<float>() * 2;
				break;
			case 1:
				m_rota->y = m_curValue.y + rad;
				if (m_rota->y >= 2*glm::pi<float>())
					m_rota->y -= (int)(m_rota->y / (2*glm::pi<float>())) * 2*glm::pi<float>();
				break;
			case 2:
				m_rota->z = m_curValue.z + rad;
				if (m_rota->z >= 2*glm::pi<float>())
					m_rota->z -= (int)(m_rota->z / (2*glm::pi<float>())) * 2*glm::pi<float>(); 
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
		glm::mat4 zWorld = glm::translate(glm::mat4(1), *m_trans) *
			glm::rotate(glm::mat4(1), glm::half_pi<float>(), glm::vec3(1, 0, 0));
		
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
			glm::vec3 maxP(GIZMO_WIDTH * GIZMO_PRECISE_COLBOX_WD * scale / 2, (GIZMO_HEIGHT + GIZMO_POINTER_HEIGHT) * scale, GIZMO_WIDTH * GIZMO_PRECISE_COLBOX_WD * scale / 2);
			glm::vec3 minP(-maxP.x, 0, -maxP.z);

			// X axis
			glm::vec3 b1 = xWorld * glm::vec4(minP, 1);
			glm::vec3 b2 = xWorld * glm::vec4(maxP, 1);

			float distX, distY, distZ, dist = std::numeric_limits<float>::infinity();
			if (ray::IntersectBox(b1, b2, rayOrigin, rayDir, distX))
				m_axisHovered = 0, dist = distX;
			else distX = std::numeric_limits<float>::infinity();

			// Y axis
			b1 = yWorld * glm::vec4(minP, 1);
			b2 = yWorld * glm::vec4(maxP, 1);

			if (ray::IntersectBox(b1, b2, rayOrigin, rayDir, distY) && distY < distX)
				m_axisHovered = 1, dist = distY;
			else distY = std::numeric_limits<float>::infinity();

			// Z axis
			b1 = zWorld * glm::vec4(minP, 1);
			b2 = zWorld * glm::vec4(maxP, 1);

			if (ray::IntersectBox(b1, b2, rayOrigin, rayDir, distZ) && distZ < distY && distZ < distX)
				m_axisHovered = 2, dist = distZ;
			else distZ = std::numeric_limits<float>::infinity();

			if (m_axisHovered != -1) {
				m_hoverDepth = dist;
				m_hoverStart = rayOrigin + m_hoverDepth * rayDir;
			}
		}
		else { // handle the rotation bars
			glm::mat4 matWorld = glm::translate(glm::mat4(1), *m_trans) *
				glm::rotate(glm::mat4(1), glm::half_pi<float>(), glm::vec3(1, 0, 0)) *
				glm::scale(glm::mat4(1), glm::vec3(scale));

			float triDist = 0;
			float curDist = std::numeric_limits<float>::infinity();
			int selIndex = -1;
			
			std::vector<std::string> models = { "RotateX", "RotateY", "RotateZ" };

			for (auto& mesh : m_model.Meshes) {
				for (int j = 0; j < models.size(); j++) {
					if (mesh.Name != models[j])
						continue;

					for (int i = 0; i < mesh.Vertices.size(); i += 3) {
						glm::vec3 v0 = matWorld * glm::vec4(mesh.Vertices[i + 0].Position, 1);// TODO: this is inefficient... idk why inverse(matWorld) isnt worked
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
				m_hoverStart = rayOrigin + rayDir*m_hoverDepth;
			}
		}
	}
	int GizmoObject::Click(int x, int y, int vw, int vh)
	{
		// handle control selection
		m_axisSelected = m_axisHovered;
		if (m_axisSelected != -1) {
			m_clickDepth = m_hoverDepth;
			m_clickStart = m_hoverStart;

			if (m_mode == 2) {
				m_clickDegrees = glm::degrees(atan2(x - (vw / 2), y - (vh / 2)));

				if (Settings::Instance().Preview.GizmoRotationUI){
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
	bool GizmoObject::Move(int x, int y, bool shift)
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

		glm::vec4 axisVec(m_axisSelected == 0, m_axisSelected == 1, m_axisSelected == 2, 0);
		glm::vec3 tAxisVec = glm::normalize(glm::mat3(invVP) * glm::vec3(axisVec));

		glm::vec4 mouseVec = rayOrigin + m_clickDepth * glm::vec4(rayDir, 0.0f);
		glm::vec3 moveVec = glm::vec3(m_clickStart - glm::vec3(mouseVec));

		float length = glm::length(moveVec);
		if (length == 0)
			return false;

		float dotval = glm::dot(glm::normalize(moveVec), glm::vec3(axisVec));

		float scale = glm::length(*m_trans - glm::vec3(SystemVariableManager::Instance().GetCamera()->GetPosition())) / GIZMO_SCALE_FACTOR;

		float moveDist = -length * dotval * (1/scale) * (1.2f + shift * 4.0f);

		if (m_mode == 0) {
			if (m_axisSelected == 0) m_tValue.x += moveDist;
			else if (m_axisSelected == 1) m_tValue.y += moveDist;
			else if (m_axisSelected == 2) m_tValue.z += moveDist;

			int snap = Settings::Instance().Preview.GizmoSnapTranslation;
			if (snap <= 0) {
				m_trans->x = m_curValue.x + m_tValue.x;
				m_trans->y = m_curValue.y + m_tValue.y;
				m_trans->z = m_curValue.z + m_tValue.z;
			} else {
				m_trans->x = m_curValue.x + ((int)m_tValue.x / snap) * snap;
				m_trans->y = m_curValue.y + ((int)m_tValue.y / snap) * snap;
				m_trans->z = m_curValue.z + ((int)m_tValue.z / snap) * snap;
			}

			ret=true;
		} else if (m_mode == 1) {
			if (m_axisSelected == 0) m_tValue.x += moveDist;
			else if (m_axisSelected == 1) m_tValue.y += moveDist;
			else if (m_axisSelected == 2) m_tValue.z += moveDist;

			int snap = Settings::Instance().Preview.GizmoSnapScale;
			if (snap <= 0) {
				m_scale->x = m_curValue.x + m_tValue.x;
				m_scale->y = m_curValue.y + m_tValue.y;
				m_scale->z = m_curValue.z + m_tValue.z;
			}
			else {
				m_scale->x = m_curValue.x + ((int)m_tValue.x / snap) * snap;
				m_scale->y = m_curValue.y + ((int)m_tValue.y / snap) * snap;
				m_scale->z = m_curValue.z + ((int)m_tValue.z / snap) * snap;
			}

			ret=true;
		}

		m_clickStart = mouseVec;

		return ret;
	}
	void GizmoObject::Render()
	{
		float scale = glm::length(*m_trans - glm::vec3(SystemVariableManager::Instance().GetCamera()->GetPosition())) / GIZMO_SCALE_FACTOR;


		glUseProgram(m_gizmoShader);
		
		glm::mat4 matWorld = glm::translate(glm::mat4(1), *m_trans)*
							 glm::rotate(glm::mat4(1), -glm::half_pi<float>(), glm::vec3(1, 0, 0)) *
							 glm::scale(glm::mat4(1), glm::vec3(scale));

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
		if (m_axisSelected != -1 && m_mode == 2 && Settings::Instance().Preview.GizmoRotationUI)
		{
			glUseProgram(m_uiShader);
			
			glm::mat4 matProj = glm::ortho(0.0f, m_vw, m_vh, 0.0f, 0.1f, 1000.0f);
			glm::mat4 uMatWVP = matProj * glm::translate(glm::mat4(1), glm::vec3(m_vw/2,m_vh/2,-1)) * glm::rotate(glm::mat4(1), glm::pi<float>(), glm::vec3(1, 0, 0));

			glUniformMatrix4fv(m_uMatWVPLoc, 1, GL_FALSE, glm::value_ptr(uMatWVP));

			glDisable(GL_DEPTH_CLAMP);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
			glBlendEquationSeparate(GL_ADD, GL_MAX);
			
			glBindVertexArray(m_uiVAO);
			glDrawArrays(GL_TRIANGLES, 0, GUI_POINT_COUNT*3);

			DefaultState::Bind();
		}
	}
}