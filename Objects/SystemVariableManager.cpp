#include "SystemVariableManager.h"
#include <glm/gtc/type_ptr.hpp>

namespace ed
{
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
		if (var->System != ed::SystemShaderVariable::None) {
			// we are using some system value so now is the right time to update its value
			bool isLastFrame = var->Flags & (char)ShaderVariable::Flag::LastFrame;
			
			if (!isLastFrame) {
				glm::mat4 rawMatrix;
				switch (var->System) {
					case ed::SystemShaderVariable::View:
						rawMatrix = SystemVariableManager::Instance().GetViewMatrix();
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
						break;
					case ed::SystemShaderVariable::Projection:
						rawMatrix = SystemVariableManager::Instance().GetProjectionMatrix();
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
						break;
					case ed::SystemShaderVariable::ViewProjection:
						rawMatrix = SystemVariableManager::Instance().GetViewProjectionMatrix();
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
						break;
					case ed::SystemShaderVariable::Orthographic:
						rawMatrix = SystemVariableManager::Instance().GetOrthographicMatrix();
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
						break;
					case ed::SystemShaderVariable::ViewOrthographic:
						rawMatrix = SystemVariableManager::Instance().GetViewOrthographicMatrix();
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
						break;
					case ed::SystemShaderVariable::GeometryTransform:
						rawMatrix = SystemVariableManager::Instance().GetGeometryTransform((PipelineItem*)item);
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
						break;
					case ed::SystemShaderVariable::ViewportSize:
					{
						glm::vec2 raw = SystemVariableManager::Instance().GetViewportSize();
						memcpy(var->Data, glm::value_ptr(raw), sizeof(glm::vec2));
					} break;
					case ed::SystemShaderVariable::MousePosition:
					{
						glm::vec2 raw = SystemVariableManager::Instance().GetMousePosition();
						memcpy(var->Data, glm::value_ptr(raw), sizeof(glm::vec2));
					} break;
					case ed::SystemShaderVariable::Mouse:
					{
						glm::vec4 raw = SystemVariableManager::Instance().GetMouse();
						memcpy(var->Data, glm::value_ptr(raw), sizeof(glm::vec4));
					} break;
					case ed::SystemShaderVariable::MouseButton:
					{
						glm::vec4 raw = SystemVariableManager::Instance().GetMouseButton();
						memcpy(var->Data, glm::value_ptr(raw), sizeof(glm::vec4));
					} break;
					case ed::SystemShaderVariable::Time:
					{
						float raw = SystemVariableManager::Instance().GetTime();
						memcpy(var->Data, &raw, sizeof(float));
					} break;
					case ed::SystemShaderVariable::TimeDelta:
					{
						float raw = SystemVariableManager::Instance().GetTimeDelta();
						memcpy(var->Data, &raw, sizeof(float));
					} break;
					case ed::SystemShaderVariable::FrameIndex:
					{
						unsigned int frame = SystemVariableManager::Instance().GetFrameIndex();
						memcpy(var->Data, &frame, sizeof(unsigned int));
					} break;
					case ed::SystemShaderVariable::IsPicked:
					{
						bool raw = SystemVariableManager::Instance().IsPicked();
						memcpy(var->Data, &raw, sizeof(bool));
					} break;
					case ed::SystemShaderVariable::CameraPosition:
					{
						glm::vec3 cam = SystemVariableManager::Instance().GetCamera()->GetPosition();
						memcpy(var->Data, glm::value_ptr(glm::vec4(cam, 1)), sizeof(glm::vec4));
					} break;
					case ed::SystemShaderVariable::CameraPosition3:
					{
						glm::vec3 cam = SystemVariableManager::Instance().GetCamera()->GetPosition();
						memcpy(var->Data, glm::value_ptr(cam), sizeof(glm::vec3));
					} break;
					case ed::SystemShaderVariable::CameraDirection3:
					{
						glm::vec3 cam = SystemVariableManager::Instance().GetCamera()->GetViewDirection();
						memcpy(var->Data, glm::value_ptr(cam), sizeof(glm::vec3));
					} break;
					case ed::SystemShaderVariable::KeysWASD:
					{
						glm::ivec4 raw = SystemVariableManager::Instance().GetKeysWASD();
						memcpy(var->Data, glm::value_ptr(raw), sizeof(glm::ivec4));
					} break;
					case ed::SystemShaderVariable::PluginVariable:
					{
						PluginSystemVariableData* pvData = &var->PluginSystemVarData;
						pvData->Owner->UpdateSystemVariableValue(var->Data, pvData->Name, (plugin::VariableType)var->GetType(), isLastFrame);
					} break;
				}
			} else {
				glm::mat4 rawMatrix;
				switch (var->System) {
					case ed::SystemShaderVariable::View:
						rawMatrix = Settings::Instance().Project.FPCamera ? m_prevState.FPCam.GetMatrix() : m_prevState.ArcCam.GetMatrix();;
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
						break;
					case ed::SystemShaderVariable::Projection:
						rawMatrix = glm::perspective(glm::radians(45.0f), m_prevState.Viewport.x / m_prevState.Viewport.y, 0.1f, 1000.0f);
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
						break;
					case ed::SystemShaderVariable::ViewProjection: {
						glm::mat4 view = Settings::Instance().Project.FPCamera ? m_prevState.FPCam.GetMatrix() : m_prevState.ArcCam.GetMatrix();;
						glm::mat4 persp = glm::perspective(glm::radians(45.0f), m_prevState.Viewport.x / m_prevState.Viewport.y, 0.1f, 1000.0f);
						
						rawMatrix = persp * view;
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
					} break;
					case ed::SystemShaderVariable::Orthographic:
						rawMatrix = glm::ortho(0.0f, m_prevState.Viewport.x, m_prevState.Viewport.y, 0.0f, 0.1f, 1000.0f);;
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
						break;
					case ed::SystemShaderVariable::ViewOrthographic: {
						glm::mat4 view = Settings::Instance().Project.FPCamera ? m_prevState.FPCam.GetMatrix() : m_prevState.ArcCam.GetMatrix();;
						glm::mat4 ortho = glm::ortho(0.0f, m_prevState.Viewport.x, m_prevState.Viewport.y, 0.0f, 0.1f, 1000.0f);;
						rawMatrix = ortho * view;
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
					} break;
					case ed::SystemShaderVariable::GeometryTransform:
						rawMatrix = m_prevGeoTransform[(PipelineItem*)item];
						memcpy(var->Data, glm::value_ptr(rawMatrix), sizeof(glm::mat4));
						break;
					case ed::SystemShaderVariable::ViewportSize:
					{
						glm::vec2 raw = m_prevState.Viewport;
						memcpy(var->Data, glm::value_ptr(raw), sizeof(glm::vec2));
					} break;
					case ed::SystemShaderVariable::MousePosition:
					{
						glm::vec2 raw = m_prevState.MousePosition;
						memcpy(var->Data, glm::value_ptr(raw), sizeof(glm::vec2));
					} break;
					case ed::SystemShaderVariable::Mouse:
					{
						glm::vec4 raw = m_prevState.Mouse;
						memcpy(var->Data, glm::value_ptr(raw), sizeof(glm::vec4));
					} break;
					case ed::SystemShaderVariable::MouseButton:
					{
						glm::vec4 raw = m_prevState.MouseButton;
						memcpy(var->Data, glm::value_ptr(raw), sizeof(glm::vec4));
					} break;
					case ed::SystemShaderVariable::Time:
					{
						float raw = SystemVariableManager::Instance().GetTime();
						memcpy(var->Data, &raw, sizeof(float));
					} break;
					case ed::SystemShaderVariable::TimeDelta:
					{
						float raw = m_prevState.DeltaTime;
						memcpy(var->Data, &raw, sizeof(float));
					} break;
					case ed::SystemShaderVariable::FrameIndex:
					{
						unsigned int frame = m_prevState.FrameIndex;
						memcpy(var->Data, &frame, sizeof(unsigned int));
					} break;
					case ed::SystemShaderVariable::IsPicked:
					{
						bool raw = m_prevState.IsPicked;
						memcpy(var->Data, &raw, sizeof(bool));
					} break;
					case ed::SystemShaderVariable::CameraPosition:
					{
						glm::vec3 cam = (Settings::Instance().Project.FPCamera ? (Camera*)&m_prevState.FPCam : (Camera*)&m_prevState.ArcCam)->GetPosition();
						memcpy(var->Data, glm::value_ptr(glm::vec4(cam, 1)), sizeof(glm::vec4));
					} break;
					case ed::SystemShaderVariable::CameraPosition3:
					{
						glm::vec3 cam = (Settings::Instance().Project.FPCamera ? (Camera*)&m_prevState.FPCam : (Camera*)&m_prevState.ArcCam)->GetPosition();
						memcpy(var->Data, glm::value_ptr(cam), sizeof(glm::vec3));
					} break;
					case ed::SystemShaderVariable::CameraDirection3:
					{
						glm::vec3 cam = (Settings::Instance().Project.FPCamera ? (Camera*)&m_prevState.FPCam : (Camera*)&m_prevState.ArcCam)->GetViewDirection();
						memcpy(var->Data, glm::value_ptr(cam), sizeof(glm::vec3));
					} break;
					case ed::SystemShaderVariable::KeysWASD:
					{
						glm::ivec4 raw = m_prevState.WASD;
						memcpy(var->Data, glm::value_ptr(raw), sizeof(glm::ivec4));
					} break;
					case ed::SystemShaderVariable::PluginVariable:
					{
						PluginSystemVariableData* pvData = &var->PluginSystemVarData;
						pvData->Owner->UpdateSystemVariableValue(var->Data, pvData->Name, (plugin::VariableType)var->GetType(), isLastFrame);
					} break;
				}
			}
		}
	}
}