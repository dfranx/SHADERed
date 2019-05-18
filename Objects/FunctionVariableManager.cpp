#include "FunctionVariableManager.h"
#include <imgui/imgui.h>
#include <DirectXMath.h>

namespace ed
{
	size_t FunctionVariableManager::GetArgumentCount(ed::FunctionShaderVariable func)
	{
		switch (func) {
			case FunctionShaderVariable::MatrixIdentity: return 0;
			case FunctionShaderVariable::MatrixLookAtLH: return 9;
			case FunctionShaderVariable::MatrixLookToLH: return 9;
			case FunctionShaderVariable::MatrixOrthographicLH: return 4;
			case FunctionShaderVariable::MatrixPerspectiveFovLH: return 4;
			case FunctionShaderVariable::MatrixPerspectiveLH: return 4;
			case FunctionShaderVariable::MatrixReflect: return 4;
			case FunctionShaderVariable::MatrixRotationAxis: return 4;
			case FunctionShaderVariable::MatrixRotationNormal: return 4;
			case FunctionShaderVariable::MatrixRotationRollPitchYaw: return 3;
			case FunctionShaderVariable::MatrixRotationX: return 1;
			case FunctionShaderVariable::MatrixRotationY: return 1;
			case FunctionShaderVariable::MatrixRotationZ: return 1;
			case FunctionShaderVariable::MatrixScaling: return 3;
			case FunctionShaderVariable::MatrixShadow: return 8;
			case FunctionShaderVariable::MatrixTranslation: return 3;
			case FunctionShaderVariable::ScalarCos: return 1;
			case FunctionShaderVariable::ScalarSin: return 1;
			case FunctionShaderVariable::VectorNormalize: return 4;
		}
	}
	void FunctionVariableManager::AllocateArgumentSpace(ed::ShaderVariable* var, ed::FunctionShaderVariable func)
	{
		size_t args = GetArgumentCount(func) * sizeof(float);
		var->Function = func;

		if (var->Arguments != nullptr) {
			free(var->Arguments);
			var->Arguments = (char*)calloc(args, 1);

			// set default values
			switch (func) {
				case FunctionShaderVariable::MatrixLookAtLH:
					*LoadFloat(var->Arguments, 5) = 1;
					*LoadFloat(var->Arguments, 7) = 1;
					break;
				case FunctionShaderVariable::MatrixLookToLH:
					*LoadFloat(var->Arguments, 5) = 1;
					*LoadFloat(var->Arguments, 7) = 1;
					break;
				case FunctionShaderVariable::MatrixPerspectiveLH:
				case FunctionShaderVariable::MatrixOrthographicLH:
					*LoadFloat(var->Arguments, 0) = 800;
					*LoadFloat(var->Arguments, 1) = 600;
					*LoadFloat(var->Arguments, 2) = 0.1f;
					*LoadFloat(var->Arguments, 3) = 100.0f;
					break;
				case FunctionShaderVariable::MatrixPerspectiveFovLH:
					*LoadFloat(var->Arguments, 0) = DirectX::XM_PIDIV2;
					*LoadFloat(var->Arguments, 1) = 800/600.0f;
					*LoadFloat(var->Arguments, 2) = 0.1f;
					*LoadFloat(var->Arguments, 3) = 100.0f;
					break;
				case FunctionShaderVariable::MatrixReflect:
					*LoadFloat(var->Arguments, 1) = -1;
					break;
				case FunctionShaderVariable::MatrixRotationAxis:
					*LoadFloat(var->Arguments, 0) = 1;
					break;
				case FunctionShaderVariable::MatrixRotationNormal:
					*LoadFloat(var->Arguments, 0) = 1;
					break;
				case FunctionShaderVariable::MatrixShadow:
					*LoadFloat(var->Arguments, 1) = -1;
					*LoadFloat(var->Arguments, 4) = 1;
					*LoadFloat(var->Arguments, 7) = 1;
					break;
				case FunctionShaderVariable::VectorNormalize:
					*LoadFloat(var->Arguments, 0) = 1;
					break;
			}
		}
		else var->Arguments = (char*)calloc(args, 1);
	}
	bool FunctionVariableManager::HasValidReturnType(ShaderVariable::ValueType ret, ed::FunctionShaderVariable func)
	{
		if (func >= FunctionShaderVariable::MatrixIdentity && func <= FunctionShaderVariable::MatrixTranslation)
			return ret == ShaderVariable::ValueType::Float4x4;
		else if (func >= FunctionShaderVariable::ScalarCos && func <= FunctionShaderVariable::ScalarSin)
			return ret == ShaderVariable::ValueType::Float1;
		else if (func == FunctionShaderVariable::VectorNormalize)
			return ret > ShaderVariable::ValueType::Float1 && ret <= ShaderVariable::ValueType::Float4;
		return false;
	}
	void FunctionVariableManager::Update(ed::ShaderVariable* var)
	{
		if (var->Function == FunctionShaderVariable::None)
			return;

		if (var->Function >= FunctionShaderVariable::MatrixIdentity && var->Function <= FunctionShaderVariable::MatrixTranslation) {
			DirectX::XMFLOAT4X4 matrix;

			switch (var->Function) {
				case FunctionShaderVariable::MatrixIdentity:
				{
					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixIdentity());
				} break;

				case FunctionShaderVariable::MatrixLookAtLH:
				{
					float* eyePos = LoadFloat(var->Arguments, 0);
					float* focus = LoadFloat(var->Arguments, 3);
					float* up = LoadFloat(var->Arguments, 6);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixLookAtLH(
						DirectX::XMVectorSet(eyePos[0], eyePos[1], eyePos[2], 0),
						DirectX::XMVectorSet(focus[0], focus[1], focus[2], 0),
						DirectX::XMVectorSet(up[0], up[1], up[2], 0)
					));
				} break;

				case FunctionShaderVariable::MatrixLookToLH:
				{
					float* eyePos = LoadFloat(var->Arguments, 0);
					float* eyeDir = LoadFloat(var->Arguments, 3);
					float* up = LoadFloat(var->Arguments, 6);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixLookToLH(
						DirectX::XMVectorSet(eyePos[0], eyePos[1], eyePos[2], 0),
						DirectX::XMVectorSet(eyeDir[0], eyeDir[1], eyeDir[2], 0),
						DirectX::XMVectorSet(up[0], up[1], up[2], 0)
					));
				} break;

				case FunctionShaderVariable::MatrixOrthographicLH:
				{
					float* view = LoadFloat(var->Arguments, 0);
					float* z = LoadFloat(var->Arguments, 2);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixOrthographicLH(
						view[0], view[1], z[0], z[1]
					));
				} break;

				case FunctionShaderVariable::MatrixPerspectiveFovLH:
				{
					float fov = *LoadFloat(var->Arguments, 0);
					float aspect = *LoadFloat(var->Arguments, 1);
					float* z = LoadFloat(var->Arguments, 2);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixPerspectiveFovLH(
						fov, aspect, z[0], z[1]
					));
				} break;

				case FunctionShaderVariable::MatrixPerspectiveLH:
				{
					float* view = LoadFloat(var->Arguments, 0);
					float* z = LoadFloat(var->Arguments, 2);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixPerspectiveLH(
						view[0], view[1], z[0], z[1]
					));
				} break;

				case FunctionShaderVariable::MatrixReflect:
				{
					float* plane = LoadFloat(var->Arguments, 0);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixReflect(
						DirectX::XMVectorSet(plane[0], plane[1], plane[2], plane[3])
					));
				} break;

				case FunctionShaderVariable::MatrixRotationAxis:
				{
					float* axis = LoadFloat(var->Arguments, 0);
					float angle = *LoadFloat(var->Arguments, 3);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixRotationAxis(
						DirectX::XMVectorSet(axis[0], axis[1], axis[2], 0),
						angle
					));
				} break;

				case FunctionShaderVariable::MatrixRotationNormal:
				{
					float* axis = LoadFloat(var->Arguments, 0);
					float angle = *LoadFloat(var->Arguments, 3);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixRotationNormal(
						DirectX::XMVectorSet(axis[0], axis[1], axis[2], 0),
						angle
					));
				} break;

				case FunctionShaderVariable::MatrixRotationX:
				{
					float angle = *LoadFloat(var->Arguments, 0);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixRotationX(
						angle
					));
				} break;

				case FunctionShaderVariable::MatrixRotationY:
				{
					float angle = *LoadFloat(var->Arguments, 0);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixRotationY(
						angle
					));
				} break;

				case FunctionShaderVariable::MatrixRotationZ:
				{
					float angle = *LoadFloat(var->Arguments, 0);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixRotationZ(
						angle
					));
				} break;

				case FunctionShaderVariable::MatrixScaling:
				{
					float* scale = LoadFloat(var->Arguments, 0);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixScaling(
						scale[0], scale[1], scale[2]
					));
				} break;

				case FunctionShaderVariable::MatrixShadow:
				{
					float* plane = LoadFloat(var->Arguments, 0);
					float* light = LoadFloat(var->Arguments, 4);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixShadow(
						DirectX::XMVectorSet(plane[0], plane[1], plane[2], plane[3]),
						DirectX::XMVectorSet(light[0], light[1], light[2], light[3])
					));
				} break;

				case FunctionShaderVariable::MatrixTranslation:
				{
					float* pos = LoadFloat(var->Arguments, 0);

					DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixTranslation(
						pos[0], pos[1], pos[2]
					));
				} break;
			}

			memcpy(var->Data, &matrix, sizeof(DirectX::XMFLOAT4X4));
		}
		else if (var->Function >= FunctionShaderVariable::ScalarCos && var->Function <= FunctionShaderVariable::ScalarSin) {
			float scalar;
			switch (var->Function) {
				case FunctionShaderVariable::ScalarCos:
				{
					scalar = DirectX::XMScalarCos(*LoadFloat(var->Arguments, 0));
				} break;

				case FunctionShaderVariable::ScalarSin:
				{
					scalar = DirectX::XMScalarSin(*LoadFloat(var->Arguments, 0));
				} break;
			}
			memcpy(var->Data, &scalar, sizeof(float));
		}
		else if (var->Function == FunctionShaderVariable::VectorNormalize) {
			DirectX::XMFLOAT4 vector;
			switch (var->Function) {
				case FunctionShaderVariable::VectorNormalize:
				{
					float* vec = LoadFloat(var->Arguments, 0);

					if (var->GetType() == ShaderVariable::ValueType::Float2)
						DirectX::XMStoreFloat4(&vector, DirectX::XMVector2Normalize(DirectX::XMVectorSet(vec[0], vec[1], 0, 0)));
					else if (var->GetType() == ShaderVariable::ValueType::Float3)
						DirectX::XMStoreFloat4(&vector, DirectX::XMVector3Normalize(DirectX::XMVectorSet(vec[0], vec[1], vec[2], 0)));
					else if (var->GetType() == ShaderVariable::ValueType::Float4)
						DirectX::XMStoreFloat4(&vector, DirectX::XMVector4Normalize(DirectX::XMVectorSet(vec[0], vec[1], vec[2], vec[3])));

				} break;
			}
			memcpy(var->Data, &vector, ShaderVariable::GetSize(var->GetType()));
		}
	}
	float * FunctionVariableManager::LoadFloat(char* data, int index)
	{
		return (float*)(data + index * sizeof(float));
	}
}
