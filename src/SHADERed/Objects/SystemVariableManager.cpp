#include <SHADERed/Objects/SystemVariableManager.h>
#include <glm/gtc/type_ptr.hpp>

namespace ed {
	void SystemVariableManager::Reset()
	{
		m_timer.Restart();
		m_curState.FrameIndex = 0;
		m_curGeoTransform.clear();
		m_prevGeoTransform.clear();
		m_advTimer = 0;
	}
	void SystemVariableManager::CopyState()
	{
		memcpy(&m_prevState, &m_curState, sizeof(m_curState));
		m_prevGeoTransform = m_curGeoTransform;
	}
	void SystemVariableManager::Update(ed::ShaderVariable* var, void* item)
	{
		// update variable's Data pointer if it's using a system value
		if (var->System != ed::SystemShaderVariable::None) {
			bool isLastFrame = var->Flags & (char)ShaderVariable::Flag::LastFrame;

			if (!isLastFrame) {
				glm::mat4 rawMatrix;
				switch (var->System) {
				case ed::SystemShaderVariable::View:
					rawMatrix = this->GetViewMatrix();
					memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
					break;
				case ed::SystemShaderVariable::Projection:
					rawMatrix = this->GetProjectionMatrix();
					memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
					break;
				case ed::SystemShaderVariable::ViewProjection:
					rawMatrix = this->GetViewProjectionMatrix();
					memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
					break;
				case ed::SystemShaderVariable::Orthographic:
					rawMatrix = this->GetOrthographicMatrix();
					memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
					break;
				case ed::SystemShaderVariable::ViewOrthographic:
					rawMatrix = this->GetViewOrthographicMatrix();
					memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
					break;
				case ed::SystemShaderVariable::GeometryTransform:
					rawMatrix = this->GetGeometryTransform((PipelineItem*)item);
					memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
					break;
				case ed::SystemShaderVariable::ViewportSize:
					memcpy(var->Data, glm::value_ptr(GetViewportSize()), sizeof(glm::vec2));
					break;
				case ed::SystemShaderVariable::MousePosition:
					memcpy(var->Data, glm::value_ptr(GetMousePosition()), sizeof(glm::vec2));
					break;
				case ed::SystemShaderVariable::Mouse:
					memcpy(var->Data, glm::value_ptr(GetMouse()), sizeof(glm::vec4));
					break;
				case ed::SystemShaderVariable::MouseButton:
					memcpy(var->Data, glm::value_ptr(GetMouseButton()), sizeof(glm::vec4));
					break;
				case ed::SystemShaderVariable::Time: {
					float raw = this->GetTime();
					memcpy(var->Data, &raw, sizeof(float));
				} break;
				case ed::SystemShaderVariable::TimeDelta:
					memcpy(var->Data, &m_curState.DeltaTime, sizeof(float));
					break;
				case ed::SystemShaderVariable::FrameIndex:
					memcpy(var->Data, &m_curState.FrameIndex, sizeof(unsigned int));
					break;
				case ed::SystemShaderVariable::IsPicked:
					memcpy(var->Data, &m_curState.IsPicked, sizeof(bool));
					break;
				case ed::SystemShaderVariable::IsSavingToFile:
					memcpy(var->Data, &m_curState.IsSavingToFile, sizeof(bool));
					break;
				case ed::SystemShaderVariable::CameraPosition: {
					glm::vec3 cam = this->GetCamera()->GetPosition();
					memcpy(var->Data, glm::value_ptr(glm::vec4(cam, 1)), sizeof(glm::vec4));
				} break;
				case ed::SystemShaderVariable::CameraPosition3: {
					glm::vec3 cam = this->GetCamera()->GetPosition();
					memcpy(var->Data, glm::value_ptr(cam), sizeof(glm::vec3));
				} break;
				case ed::SystemShaderVariable::CameraDirection3: {
					glm::vec3 cam = this->GetCamera()->GetViewDirection();
					memcpy(var->Data, glm::value_ptr(cam), sizeof(glm::vec3));
				} break;
				case ed::SystemShaderVariable::KeysWASD:
					memcpy(var->Data, glm::value_ptr(this->GetKeysWASD()), sizeof(glm::ivec4));
					break;
				case ed::SystemShaderVariable::PickPosition:
					memcpy(var->Data, glm::value_ptr(this->GetPickPosition()), sizeof(glm::vec3));
					break;
				case ed::SystemShaderVariable::PluginVariable: {
					PluginSystemVariableData* pvData = &var->PluginSystemVarData;
					pvData->Owner->SystemVariables_UpdateValue(var->Data, pvData->Name, (plugin::VariableType)var->GetType(), isLastFrame);
				} break;
				}
			} else {
				glm::mat4 rawMatrix;
				switch (var->System) {
				case ed::SystemShaderVariable::View:
					rawMatrix = Settings::Instance().Project.FPCamera ? m_prevState.FPCam.GetMatrix() : m_prevState.ArcCam.GetMatrix();
					memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
					break;
				case ed::SystemShaderVariable::Projection:
					rawMatrix = glm::perspective(glm::radians(45.0f), m_prevState.Viewport.x / m_prevState.Viewport.y, 0.1f, 1000.0f);
					memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
					break;
				case ed::SystemShaderVariable::ViewProjection: {
					glm::mat4 view = Settings::Instance().Project.FPCamera ? m_prevState.FPCam.GetMatrix() : m_prevState.ArcCam.GetMatrix();
					glm::mat4 persp = glm::perspective(glm::radians(45.0f), m_prevState.Viewport.x / m_prevState.Viewport.y, 0.1f, 1000.0f);

					rawMatrix = persp * view;
					memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
				} break;
				case ed::SystemShaderVariable::Orthographic:
					rawMatrix = glm::ortho(0.0f, m_prevState.Viewport.x, m_prevState.Viewport.y, 0.0f, 0.1f, 1000.0f);
					memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
					break;
				case ed::SystemShaderVariable::ViewOrthographic: {
					glm::mat4 view = Settings::Instance().Project.FPCamera ? m_prevState.FPCam.GetMatrix() : m_prevState.ArcCam.GetMatrix();
					glm::mat4 ortho = glm::ortho(0.0f, m_prevState.Viewport.x, m_prevState.Viewport.y, 0.0f, 0.1f, 1000.0f);
					rawMatrix = ortho * view;
					memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
				} break;
				case ed::SystemShaderVariable::GeometryTransform:
					memcpy(var->Data, glm::value_ptr(m_prevGeoTransform[(PipelineItem*)item]), sizeof(glm::mat4));
					break;
				case ed::SystemShaderVariable::ViewportSize:
					memcpy(var->Data, glm::value_ptr(m_prevState.Viewport), sizeof(glm::vec2));
					break;
				case ed::SystemShaderVariable::MousePosition:
					memcpy(var->Data, glm::value_ptr(m_prevState.MousePosition), sizeof(glm::vec2));
					break;
				case ed::SystemShaderVariable::Mouse:
					memcpy(var->Data, glm::value_ptr(m_prevState.Mouse), sizeof(glm::vec4));
					break;
				case ed::SystemShaderVariable::MouseButton:
					memcpy(var->Data, glm::value_ptr(m_prevState.MouseButton), sizeof(glm::vec4));
					break;
				case ed::SystemShaderVariable::Time: {
					float raw = SystemVariableManager::Instance().GetTime();
					memcpy(var->Data, &raw, sizeof(float));
				} break;
				case ed::SystemShaderVariable::TimeDelta:
					memcpy(var->Data, &m_prevState.DeltaTime, sizeof(float));
					break;
				case ed::SystemShaderVariable::FrameIndex:
					memcpy(var->Data, &m_prevState.FrameIndex, sizeof(unsigned int));
					break;
				case ed::SystemShaderVariable::IsPicked:
					memcpy(var->Data, &m_prevState.IsPicked, sizeof(bool));
					break;
				case ed::SystemShaderVariable::IsSavingToFile:
					memcpy(var->Data, &m_prevState.IsSavingToFile, sizeof(bool));
					break;
				case ed::SystemShaderVariable::CameraPosition: {
					glm::vec3 cam = (Settings::Instance().Project.FPCamera ? (Camera*)&m_prevState.FPCam : (Camera*)&m_prevState.ArcCam)->GetPosition();
					memcpy(var->Data, glm::value_ptr(glm::vec4(cam, 1)), sizeof(glm::vec4));
				} break;
				case ed::SystemShaderVariable::CameraPosition3: {
					glm::vec3 cam = (Settings::Instance().Project.FPCamera ? (Camera*)&m_prevState.FPCam : (Camera*)&m_prevState.ArcCam)->GetPosition();
					memcpy(var->Data, glm::value_ptr(cam), sizeof(glm::vec3));
				} break;
				case ed::SystemShaderVariable::CameraDirection3: {
					glm::vec3 cam = (Settings::Instance().Project.FPCamera ? (Camera*)&m_prevState.FPCam : (Camera*)&m_prevState.ArcCam)->GetViewDirection();
					memcpy(var->Data, glm::value_ptr(cam), sizeof(glm::vec3));
				} break;
				case ed::SystemShaderVariable::KeysWASD:
					memcpy(var->Data, glm::value_ptr(m_prevState.WASD), sizeof(glm::ivec4));
					break;
				case ed::SystemShaderVariable::PickPosition:
					memcpy(var->Data, glm::value_ptr(m_prevState.PickPosition), sizeof(glm::vec3));
					break;
				case ed::SystemShaderVariable::PluginVariable: {
					PluginSystemVariableData* pvData = &var->PluginSystemVarData;
					pvData->Owner->SystemVariables_UpdateValue(var->Data, pvData->Name, (plugin::VariableType)var->GetType(), isLastFrame);
				} break;
				}
			}
		}
	}
	ed::SystemShaderVariable SystemVariableManager::GetTypeFromName(const std::string& name)
	{
		std::string vname = name;
		std::transform(vname.begin(), vname.end(), vname.begin(), tolower);

		// list of rules for detection:
		if (vname.find("time") != std::string::npos && vname.find("d") == std::string::npos && vname.find("del") == std::string::npos && vname.find("delta") == std::string::npos)
			return SystemShaderVariable::Time;
		else if (vname.find("time") != std::string::npos && (vname.find("d") != std::string::npos || vname.find("del") != std::string::npos || vname.find("delta") != std::string::npos))
			return SystemShaderVariable::TimeDelta;
		else if (vname.find("frame") != std::string::npos || vname.find("index") != std::string::npos)
			return SystemShaderVariable::FrameIndex;
		else if (vname.find("size") != std::string::npos || vname.find("window") != std::string::npos || vname.find("viewport") != std::string::npos || vname.find("resolution") != std::string::npos || vname.find("res") != std::string::npos)
			return SystemShaderVariable::ViewportSize;
		else if (vname.find("mouse") != std::string::npos || vname == "mpos")
			return SystemShaderVariable::MousePosition;
		else if (vname == "view" || vname == "matview" || vname == "matv" || vname == "mview")
			return SystemShaderVariable::View;
		else if (vname == "proj" || vname == "matproj" || vname == "matp" || vname == "mproj" || vname == "projection" || vname == "matprojection" || vname == "mprojection")
			return SystemShaderVariable::Projection;
		else if (vname == "matvp" || vname == "matviewproj" || vname == "matviewprojection" || vname == "viewprojection" || vname == "viewproj" || vname == "mvp")
			return SystemShaderVariable::ViewProjection;
		else if (vname.find("ortho") != std::string::npos)
			return SystemShaderVariable::Orthographic;
		else if (vname.find("geo") != std::string::npos || vname.find("model") != std::string::npos)
			return SystemShaderVariable::GeometryTransform;
		else if (vname.find("picked") != std::string::npos)
			return SystemShaderVariable::IsPicked;
		else if (vname.find("pick") != std::string::npos)
			return SystemShaderVariable::PickPosition;
		else if (vname.find("issaving") != std::string::npos)
			return SystemShaderVariable::IsSavingToFile;
		else if (vname.find("cam") != std::string::npos)
			return SystemShaderVariable::CameraPosition3;
		else if (vname.find("keys") != std::string::npos || vname.find("wasd") != std::string::npos)
			return SystemShaderVariable::KeysWASD;

		return SystemShaderVariable::None;
	}
}