#include "FunctionVariableManager.h"
#include "CameraSnapshots.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace ed
{
	int FunctionVariableManager::CurrentIndex = 0;
	std::vector<ed::ShaderVariable*> FunctionVariableManager::VariableList = std::vector<ed::ShaderVariable*>();

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
			case FunctionShaderVariable::Pointer: return 1;
			case FunctionShaderVariable::CameraSnapshot: return 1;
			case FunctionShaderVariable::VectorNormalize: return 4;
		}
		return 0;
	}
	void FunctionVariableManager::AllocateArgumentSpace(ed::ShaderVariable* var, ed::FunctionShaderVariable func)
	{
		size_t args = GetArgumentCount(func) * sizeof(float);
		if (func == FunctionShaderVariable::PluginFunction)
			args = var->PluginFuncData.Owner->GetVariableFunctionArgSpaceSize(var->PluginFuncData.Name, (plugin::VariableType)var->GetType());
		var->Function = func;

		if (func == ed::FunctionShaderVariable::Pointer || func == ed::FunctionShaderVariable::CameraSnapshot)
			args = sizeof(char) * VARIABLE_NAME_LENGTH;

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
					*LoadFloat(var->Arguments, 0) = glm::half_pi<float>();
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
				case FunctionShaderVariable::Pointer:
					memset(var->Arguments, 0, VARIABLE_NAME_LENGTH*sizeof(char));
					break;
				case FunctionShaderVariable::CameraSnapshot:
					memset(var->Arguments, 0, VARIABLE_NAME_LENGTH*sizeof(char));
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
		else if (func == FunctionShaderVariable::CameraSnapshot)
			return ret == ShaderVariable::ValueType::Float4x4;
		else if (func == FunctionShaderVariable::Pointer)
			return true;
		return false;
	}
	void FunctionVariableManager::AddToList(ed::ShaderVariable* var)
	{
		int& curIndex = FunctionVariableManager::CurrentIndex;
		std::vector<ed::ShaderVariable*>& varList = FunctionVariableManager::VariableList;
		for (int i = 0; i < curIndex; i++)
			if(varList[i] == var)
				return; // already exists
		if (curIndex % 20 == 0)
			varList.resize(curIndex+20);
		varList[curIndex] = var;
		curIndex++;
	}
	void FunctionVariableManager::Update(ed::ShaderVariable* var)
	{

		if (var->Function == FunctionShaderVariable::None)
			return;

		if (var->Function == FunctionShaderVariable::Pointer) {
			std::vector<ed::ShaderVariable*>& varList = FunctionVariableManager::VariableList;
			for (int i = 0; i < varList.size(); i++) {
				if (varList[i] == nullptr)
					continue;

				if (strcmp(varList[i]->Name, var->Arguments) == 0) {
					memcpy(var->Data, varList[i]->Data, ShaderVariable::GetSize(var->GetType()));
					break;
				}
			}
		}
		else if (var->Function == FunctionShaderVariable::CameraSnapshot) {
			glm::mat4 camVal = CameraSnapshots::Get(var->Arguments);
			memcpy(var->Data, glm::value_ptr(camVal), sizeof(glm::mat4));
		}
		else if (var->Function >= FunctionShaderVariable::MatrixIdentity && var->Function <= FunctionShaderVariable::MatrixTranslation) {
			glm::mat4 matrix;

			switch (var->Function) {
				case FunctionShaderVariable::MatrixIdentity:
				{
					matrix = glm::mat4(1.0f);
				} break;

				case FunctionShaderVariable::MatrixLookAtLH:
				{
					float* eyePos = LoadFloat(var->Arguments, 0);
					float* focus = LoadFloat(var->Arguments, 3);
					float* up = LoadFloat(var->Arguments, 6);

					matrix = glm::lookAt(glm::make_vec3(eyePos),
										 glm::make_vec3(focus),
										 glm::make_vec3(up));
				} break;

				case FunctionShaderVariable::MatrixLookToLH:
				{
					float* eyePos = LoadFloat(var->Arguments, 0);
					float* eyeDir = LoadFloat(var->Arguments, 3);
					float* up = LoadFloat(var->Arguments, 6);

					matrix = glm::lookAt(glm::make_vec3(eyePos),
										 glm::make_vec3(eyePos) + glm::make_vec3(eyeDir),
										 glm::make_vec3(up));
				} break;

				case FunctionShaderVariable::MatrixOrthographicLH:
				{
					float* view = LoadFloat(var->Arguments, 0);
					float* z = LoadFloat(var->Arguments, 2);

					matrix = glm::ortho(0.0f, view[0], 0.0f, view[1], z[0], z[1]);
				} break;

				case FunctionShaderVariable::MatrixPerspectiveFovLH:
				{
					float fov = *LoadFloat(var->Arguments, 0);
					float aspect = *LoadFloat(var->Arguments, 1);
					float* z = LoadFloat(var->Arguments, 2);

					matrix = glm::perspective(fov, aspect, z[0], z[1]);
				} break;

				case FunctionShaderVariable::MatrixPerspectiveLH:
				{
					float* view = LoadFloat(var->Arguments, 0);
					float* z = LoadFloat(var->Arguments, 2);

					matrix = glm::perspectiveFov(45.0f, view[0], view[1], z[0], z[1]);
				} break;

				case FunctionShaderVariable::MatrixReflect:
				{
					float* plane = LoadFloat(var->Arguments, 0);

					// TODO: implement this!
					matrix = glm::mat4(1.0f);
				} break;

				case FunctionShaderVariable::MatrixRotationAxis:
				{
					float* axis = LoadFloat(var->Arguments, 0);
					float angle = *LoadFloat(var->Arguments, 3);

					matrix = glm::rotate(glm::mat4(1), glm::radians(angle), glm::make_vec3(axis));
				} break;

				case FunctionShaderVariable::MatrixRotationNormal:
				{
					float* axis = LoadFloat(var->Arguments, 0);
					float angle = *LoadFloat(var->Arguments, 3);

					// TODO: remove this
					matrix = glm::rotate(glm::mat4(1), glm::radians(angle), glm::make_vec3(axis));
				} break;

				case FunctionShaderVariable::MatrixRotationX:
				{
					float angle = *LoadFloat(var->Arguments, 0);

					matrix = glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(1,0,0));
				} break;

				case FunctionShaderVariable::MatrixRotationY:
				{
					float angle = *LoadFloat(var->Arguments, 0);

					matrix = glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(0,1,0));
				} break;

				case FunctionShaderVariable::MatrixRotationZ:
				{
					float angle = *LoadFloat(var->Arguments, 0);

					
					matrix = glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(0,0,1));
				} break;

				case FunctionShaderVariable::MatrixScaling:
				{
					float* scale = LoadFloat(var->Arguments, 0);

					matrix = glm::scale(glm::mat4(1), glm::make_vec3(scale));
				} break;

				case FunctionShaderVariable::MatrixShadow:
				{
					float* plane = LoadFloat(var->Arguments, 0);
					float* light = LoadFloat(var->Arguments, 4);

					// TODO: implement this
					matrix = glm::mat4(1.0f);
				} break;

				case FunctionShaderVariable::MatrixTranslation:
				{
					float* pos = LoadFloat(var->Arguments, 0);

					matrix = glm::translate(glm::mat4(1), glm::make_vec3(pos));
				} break;
			}

			memcpy(var->Data, glm::value_ptr(matrix), sizeof(glm::mat4));
		}
		else if (var->Function >= FunctionShaderVariable::ScalarCos && var->Function <= FunctionShaderVariable::ScalarSin) {
			float scalar;
			switch (var->Function) {
				case FunctionShaderVariable::ScalarCos:
				{
					scalar = glm::cos(glm::radians(*LoadFloat(var->Arguments, 0)));
				} break;

				case FunctionShaderVariable::ScalarSin:
				{
					scalar = glm::sin(glm::radians(*LoadFloat(var->Arguments, 0)));
				} break;
			}
			memcpy(var->Data, &scalar, sizeof(float));
		}
		else if (var->Function == FunctionShaderVariable::VectorNormalize) {
			glm::vec4 vector;
			switch (var->Function) {
				case FunctionShaderVariable::VectorNormalize:
				{
					float* vec = LoadFloat(var->Arguments, 0);

					if (var->GetType() == ShaderVariable::ValueType::Float2)
						vector = glm::vec4(glm::normalize(glm::make_vec2(vec)), 0, 0);
					else if (var->GetType() == ShaderVariable::ValueType::Float3)
						vector = glm::vec4(glm::normalize(glm::make_vec3(vec)), 0);
					else if (var->GetType() == ShaderVariable::ValueType::Float4)
						vector = glm::normalize(glm::make_vec4(vec));

				} break;
			}
			memcpy(var->Data, &vector, ShaderVariable::GetSize(var->GetType()));
		}
		else if (var->Function == FunctionShaderVariable::PluginFunction)
			var->PluginFuncData.Owner->UpdateVariableFunctionValue(var->Data, var->Arguments, var->PluginFuncData.Name, (plugin::VariableType)var->GetType());
	}
	void FunctionVariableManager::ClearVariableList()
	{
		FunctionVariableManager::VariableList.clear();
		FunctionVariableManager::CurrentIndex = 0;
	}
	float * FunctionVariableManager::LoadFloat(char* data, int index)
	{
		return (float*)(data + index * sizeof(float));
	}
}
