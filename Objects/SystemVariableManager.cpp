#include "SystemVariableManager.h"

namespace ed
{
	void SystemVariableManager::Update(ed::ShaderVariable* var)
	{
		if (var->System != ed::SystemShaderVariable::None) {
			// we are using some system value so now is the right time to update its value

			DirectX::XMFLOAT4X4 rawMatrix;

			switch (var->System) {
				case ed::SystemShaderVariable::View:
					DirectX::XMStoreFloat4x4(&rawMatrix, DirectX::XMMatrixTranspose(SystemVariableManager::Instance().GetViewMatrix()));
					memcpy(var->Data, &rawMatrix, sizeof(DirectX::XMFLOAT4X4));
					break;
				case ed::SystemShaderVariable::Projection:
					DirectX::XMStoreFloat4x4(&rawMatrix, DirectX::XMMatrixTranspose(SystemVariableManager::Instance().GetProjectionMatrix()));
					memcpy(var->Data, &rawMatrix, sizeof(DirectX::XMFLOAT4X4));
					break;
				case ed::SystemShaderVariable::ViewProjection:
					DirectX::XMStoreFloat4x4(&rawMatrix, DirectX::XMMatrixTranspose(SystemVariableManager::Instance().GetViewProjectionMatrix()));
					memcpy(var->Data, &rawMatrix, sizeof(DirectX::XMFLOAT4X4));
					break;
				case ed::SystemShaderVariable::Orthographic:
					DirectX::XMStoreFloat4x4(&rawMatrix, DirectX::XMMatrixTranspose(SystemVariableManager::Instance().GetOrthographicMatrix()));
					memcpy(var->Data, &rawMatrix, sizeof(DirectX::XMFLOAT4X4));
					break;
				case ed::SystemShaderVariable::ViewOrthographic:
					DirectX::XMStoreFloat4x4(&rawMatrix, DirectX::XMMatrixTranspose(SystemVariableManager::Instance().GetViewOrthographicMatrix()));
					memcpy(var->Data, &rawMatrix, sizeof(DirectX::XMFLOAT4X4));
					break;
				case ed::SystemShaderVariable::GeometryTransform:
					DirectX::XMStoreFloat4x4(&rawMatrix, DirectX::XMMatrixTranspose(SystemVariableManager::Instance().GetGeometryTransform()));
					memcpy(var->Data, &rawMatrix, sizeof(DirectX::XMFLOAT4X4));
					break;
				case ed::SystemShaderVariable::ViewportSize:
				{
					DirectX::XMFLOAT2 raw = SystemVariableManager::Instance().GetViewportSize();
					memcpy(var->Data, &raw, sizeof(DirectX::XMFLOAT2));
				} break;
				case ed::SystemShaderVariable::MousePosition:
				{
					DirectX::XMFLOAT2 raw = SystemVariableManager::Instance().GetMousePosition();
					memcpy(var->Data, &raw, sizeof(DirectX::XMFLOAT2));
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
					DirectX::XMFLOAT3 cam;
					DirectX::XMStoreFloat3(&cam, SystemVariableManager::Instance().GetCamera()->GetPosition());
					DirectX::XMFLOAT4 raw(cam.x, cam.y, cam.z, 1);
					memcpy(var->Data, &raw, sizeof(DirectX::XMFLOAT4));
				} break;
				case ed::SystemShaderVariable::KeysWASD:
				{
					DirectX::XMINT4 raw = SystemVariableManager::Instance().GetKeysWASD();
					memcpy(var->Data, &raw, sizeof(DirectX::XMFLOAT3));
				} break;
			}
		}
	}
}