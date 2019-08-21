#include "SystemVariableManager.h"
#include <glm/gtc/type_ptr.hpp>

namespace ed
{
	void SystemVariableManager::Update(ed::ShaderVariable* var)
	{
		if (var->System != ed::SystemShaderVariable::None) {
			// we are using some system value so now is the right time to update its value
			
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
					rawMatrix = SystemVariableManager::Instance().GetGeometryTransform();
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
				case ed::SystemShaderVariable::IsPicked:
				{
					bool raw = SystemVariableManager::Instance().IsPicked();
					memcpy(var->Data, &raw, sizeof(bool));
				} break;
				case ed::SystemShaderVariable::CameraPosition:
				{
					glm::vec3 cam = SystemVariableManager::Instance().GetCamera()->GetPosition();
					memcpy(var->Data, glm::value_ptr(glm::vec4(cam, 1)), sizeof(glm::vec3));
				} break;
				case ed::SystemShaderVariable::KeysWASD:
				{
					glm::ivec4 raw = SystemVariableManager::Instance().GetKeysWASD();
					memcpy(var->Data, glm::value_ptr(raw), sizeof(glm::ivec4));
				} break;
			}
		}
	}
}