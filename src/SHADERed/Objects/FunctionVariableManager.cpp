#include <SHADERed/Objects/CameraSnapshots.h>
#include <SHADERed/Objects/FunctionVariableManager.h>
#include <SHADERed/Objects/DebugInformation.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace ed {
	FunctionVariableManager::FunctionVariableManager()
	{
		m_currentIndex = 0;
	}

	void FunctionVariableManager::Initialize(PipelineManager* pipe, DebugInformation* dbgr, RenderEngine* rndr)
	{
		m_pipeline = pipe;
		m_debugger = dbgr;
		m_renderer = rndr;
		m_currentIndex = 0;
	}

	void FunctionVariableManager::AddToList(ed::ShaderVariable* var)
	{
		for (int i = 0; i < m_currentIndex; i++)
			if (VariableList[i] == var)
				return; // already exists
		if (m_currentIndex % 20 == 0)
			VariableList.resize(m_currentIndex + 20);
		VariableList[m_currentIndex] = var;
		m_currentIndex++;
	}
	void FunctionVariableManager::Update(ed::ShaderVariable* var)
	{
		if (var->Function == FunctionShaderVariable::None || var->System != SystemShaderVariable::None)
			return;

		if (var->Function == FunctionShaderVariable::Pointer) {
			for (int i = 0; i < VariableList.size(); i++) {
				if (VariableList[i] == nullptr)
					continue;

				if (strcmp(VariableList[i]->Name, var->Arguments) == 0) {
					memcpy(var->Data, VariableList[i]->Data, ShaderVariable::GetSize(var->GetType()));
					break;
				}
			}
		} else if (var->Function == FunctionShaderVariable::CameraSnapshot) {
			glm::mat4 camVal = CameraSnapshots::Get(var->Arguments);
			memcpy(var->Data, glm::value_ptr(camVal), sizeof(glm::mat4));
		} else if (var->Function == FunctionShaderVariable::ObjectProperty) {
			ed::PipelineItem* item = m_pipeline->Get(var->Arguments);

			if (item == nullptr)
				return;

			char* propName = var->Arguments + PIPELINE_ITEM_NAME_LENGTH;
			glm::vec4 dataVal(0.0f);

			if (item->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* geom = (pipe::GeometryItem*)item->Data;

				if (strcmp(propName, "Position") == 0)
					dataVal = glm::vec4(geom->Position, 1.0f);
				else if (strcmp(propName, "Scale") == 0)
					dataVal = glm::vec4(geom->Scale, 0.0f);
				else if (strcmp(propName, "Rotation") == 0)
					dataVal = glm::vec4(geom->Rotation, 0.0f);
				else if (strcmp(propName, "VertexShaderPosition") == 0) {
					if (m_vertexShaderTimer[item->Data].GetElapsedTime() < 1.0f / 30.0f)
						dataVal = m_vertexShaderCache[item->Data];
					else {
						PipelineItem* passItem = m_pipeline->Get(m_pipeline->GetItemOwner(item->Name));
						m_vertexShaderCache[item->Data] = dataVal = m_debugger->GetPositionThroughVertexShader(passItem, item, glm::vec3(0.0f));
						m_vertexShaderTimer[item->Data].Restart();
					}
				}
			} else if (item->Type == PipelineItem::ItemType::Model) {
				pipe::Model* mdl = (pipe::Model*)item->Data;

				if (strcmp(propName, "Position") == 0)
					dataVal = glm::vec4(mdl->Position, 1.0f);
				else if (strcmp(propName, "Scale") == 0)
					dataVal = glm::vec4(mdl->Scale, 0.0f);
				else if (strcmp(propName, "Rotation") == 0)
					dataVal = glm::vec4(mdl->Rotation, 0.0f);
				else if (strcmp(propName, "VertexShaderPosition") == 0) {
					if (m_vertexShaderTimer[item->Data].GetElapsedTime() < 1.0f / 30.0f)
						dataVal = m_vertexShaderCache[item->Data];
					else {
						PipelineItem* passItem = m_pipeline->Get(m_pipeline->GetItemOwner(item->Name));
						m_vertexShaderCache[item->Data] = dataVal = m_debugger->GetPositionThroughVertexShader(passItem, item, glm::vec3(0.0f));
						m_vertexShaderTimer[item->Data].Restart();
					}
				}
			} else if (item->Type == PipelineItem::ItemType::VertexBuffer) {
				pipe::VertexBuffer* vbuf = (pipe::VertexBuffer*)item->Data;

				if (strcmp(propName, "Position") == 0)
					dataVal = glm::vec4(vbuf->Position, 1.0f);
				else if (strcmp(propName, "Scale") == 0)
					dataVal = glm::vec4(vbuf->Scale, 0.0f);
				else if (strcmp(propName, "Rotation") == 0)
					dataVal = glm::vec4(vbuf->Rotation, 0.0f);
				else if (strcmp(propName, "VertexShaderPosition") == 0) {
					if (m_vertexShaderTimer[item->Data].GetElapsedTime() < 1.0f / 30.0f)
						dataVal = m_vertexShaderCache[item->Data];
					else {
						PipelineItem* passItem = m_pipeline->Get(m_pipeline->GetItemOwner(item->Name));
						m_vertexShaderCache[item->Data] = dataVal = m_debugger->GetPositionThroughVertexShader(passItem, item, glm::vec3(0.0f));
						m_vertexShaderTimer[item->Data].Restart();
					}
				}
			}

			if (var->GetType() == ShaderVariable::ValueType::Float3)
				memcpy(var->Data, glm::value_ptr(dataVal), sizeof(glm::vec3));
			else if (var->GetType() == ShaderVariable::ValueType::Float4)
				memcpy(var->Data, glm::value_ptr(dataVal), sizeof(glm::vec4));
		} else if (var->Function >= FunctionShaderVariable::MatrixIdentity && var->Function <= FunctionShaderVariable::MatrixTranslation) {
			glm::mat4 matrix;

			switch (var->Function) {
			case FunctionShaderVariable::MatrixIdentity: {
				matrix = glm::mat4(1.0f);
			} break;

			case FunctionShaderVariable::MatrixLookAtLH: {
				float* eyePos = LoadFloat(var->Arguments, 0);
				float* focus = LoadFloat(var->Arguments, 3);
				float* up = LoadFloat(var->Arguments, 6);

				matrix = glm::lookAt(glm::make_vec3(eyePos),
					glm::make_vec3(focus),
					glm::make_vec3(up));
			} break;

			case FunctionShaderVariable::MatrixLookToLH: {
				float* eyePos = LoadFloat(var->Arguments, 0);
				float* eyeDir = LoadFloat(var->Arguments, 3);
				float* up = LoadFloat(var->Arguments, 6);

				matrix = glm::lookAt(glm::make_vec3(eyePos),
					glm::make_vec3(eyePos) + glm::make_vec3(eyeDir),
					glm::make_vec3(up));
			} break;

			case FunctionShaderVariable::MatrixOrthographicLH: {
				float* view = LoadFloat(var->Arguments, 0);
				float* z = LoadFloat(var->Arguments, 2);

				matrix = glm::ortho(0.0f, view[0], 0.0f, view[1], z[0], z[1]);
			} break;

			case FunctionShaderVariable::MatrixPerspectiveFovLH: {
				float fov = *LoadFloat(var->Arguments, 0);
				float aspect = *LoadFloat(var->Arguments, 1);
				float* z = LoadFloat(var->Arguments, 2);

				matrix = glm::perspective(fov, aspect, z[0], z[1]);
			} break;

			case FunctionShaderVariable::MatrixPerspectiveLH: {
				float* view = LoadFloat(var->Arguments, 0);
				float* z = LoadFloat(var->Arguments, 2);

				matrix = glm::perspectiveFov(45.0f, view[0], view[1], z[0], z[1]);
			} break;

			case FunctionShaderVariable::MatrixRotationAxis: {
				float* axis = LoadFloat(var->Arguments, 0);
				float angle = *LoadFloat(var->Arguments, 3);

				matrix = glm::rotate(glm::mat4(1), glm::radians(angle), glm::make_vec3(axis));
			} break;

			case FunctionShaderVariable::MatrixRotationNormal: {
				float* axis = LoadFloat(var->Arguments, 0);
				float angle = *LoadFloat(var->Arguments, 3);

				// TODO: remove this
				matrix = glm::rotate(glm::mat4(1), glm::radians(angle), glm::make_vec3(axis));
			} break;

			case FunctionShaderVariable::MatrixRotationX: {
				float angle = *LoadFloat(var->Arguments, 0);

				matrix = glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(1, 0, 0));
			} break;

			case FunctionShaderVariable::MatrixRotationY: {
				float angle = *LoadFloat(var->Arguments, 0);

				matrix = glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(0, 1, 0));
			} break;

			case FunctionShaderVariable::MatrixRotationZ: {
				float angle = *LoadFloat(var->Arguments, 0);

				matrix = glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(0, 0, 1));
			} break;

			case FunctionShaderVariable::MatrixScaling: {
				float* scale = LoadFloat(var->Arguments, 0);

				matrix = glm::scale(glm::mat4(1), glm::make_vec3(scale));
			} break;

			case FunctionShaderVariable::MatrixTranslation: {
				float* pos = LoadFloat(var->Arguments, 0);

				matrix = glm::translate(glm::mat4(1), glm::make_vec3(pos));
			} break;
			}

			memcpy(var->Data, glm::value_ptr(matrix), sizeof(glm::mat4));
		} else if (var->Function >= FunctionShaderVariable::ScalarCos && var->Function <= FunctionShaderVariable::ScalarSin) {
			float scalar;
			switch (var->Function) {
			case FunctionShaderVariable::ScalarCos: {
				scalar = glm::cos(glm::radians(*LoadFloat(var->Arguments, 0)));
			} break;

			case FunctionShaderVariable::ScalarSin: {
				scalar = glm::sin(glm::radians(*LoadFloat(var->Arguments, 0)));
			} break;
			}
			memcpy(var->Data, &scalar, sizeof(float));
		} else if (var->Function == FunctionShaderVariable::VectorNormalize) {
			glm::vec4 vector;
			switch (var->Function) {
			case FunctionShaderVariable::VectorNormalize: {
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
		} else if (var->Function == FunctionShaderVariable::PluginFunction)
			var->PluginFuncData.Owner->VariableFunctions_UpdateValue(var->Data, var->Arguments, var->PluginFuncData.Name, (plugin::VariableType)var->GetType());
	}
	void FunctionVariableManager::ClearVariableList()
	{
		for (auto it = m_vertexShaderTimer.begin(); it != m_vertexShaderTimer.end();) {
			if (it->second.GetElapsedTime() > 1.0f) {
				if (m_vertexShaderCache.count(it->first))
					m_vertexShaderCache.erase(it->first);
				it = m_vertexShaderTimer.erase(it);
			}
			else
				++it;
		}
		VariableList.clear();
		m_currentIndex = 0;
	}

	void FunctionVariableManager::AllocateArgumentSpace(ed::ShaderVariable* var, ed::FunctionShaderVariable func)
	{
		size_t args = GetArgumentCount(func) * sizeof(float);
		if (func == FunctionShaderVariable::PluginFunction)
			args = var->PluginFuncData.Owner->VariableFunctions_GetArgsSize(var->PluginFuncData.Name, (plugin::VariableType)var->GetType());
		var->Function = func;

		if (func == ed::FunctionShaderVariable::Pointer || func == ed::FunctionShaderVariable::CameraSnapshot || func == ed::FunctionShaderVariable::ObjectProperty)
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
				*LoadFloat(var->Arguments, 1) = 800 / 600.0f;
				*LoadFloat(var->Arguments, 2) = 0.1f;
				*LoadFloat(var->Arguments, 3) = 100.0f;
				break;
			case FunctionShaderVariable::MatrixRotationAxis:
				*LoadFloat(var->Arguments, 0) = 1;
				break;
			case FunctionShaderVariable::MatrixRotationNormal:
				*LoadFloat(var->Arguments, 0) = 1;
				break;
			case FunctionShaderVariable::VectorNormalize:
				*LoadFloat(var->Arguments, 0) = 1;
				break;
			case FunctionShaderVariable::CameraSnapshot:
			case FunctionShaderVariable::ObjectProperty:
			case FunctionShaderVariable::Pointer:
				memset(var->Arguments, 0, VARIABLE_NAME_LENGTH * sizeof(char));
				break;
			}
		} else
			var->Arguments = (char*)calloc(args, 1);
	}
	size_t FunctionVariableManager::GetArgumentCount(ed::FunctionShaderVariable func)
	{
		switch (func) {
		case FunctionShaderVariable::MatrixIdentity: return 0;
		case FunctionShaderVariable::MatrixLookAtLH: return 9;
		case FunctionShaderVariable::MatrixLookToLH: return 9;
		case FunctionShaderVariable::MatrixOrthographicLH: return 4;
		case FunctionShaderVariable::MatrixPerspectiveFovLH: return 4;
		case FunctionShaderVariable::MatrixPerspectiveLH: return 4;
		case FunctionShaderVariable::MatrixRotationAxis: return 4;
		case FunctionShaderVariable::MatrixRotationNormal: return 4;
		case FunctionShaderVariable::MatrixRotationRollPitchYaw: return 3;
		case FunctionShaderVariable::MatrixRotationX: return 1;
		case FunctionShaderVariable::MatrixRotationY: return 1;
		case FunctionShaderVariable::MatrixRotationZ: return 1;
		case FunctionShaderVariable::MatrixScaling: return 3;
		case FunctionShaderVariable::MatrixTranslation: return 3;
		case FunctionShaderVariable::ScalarCos: return 1;
		case FunctionShaderVariable::ScalarSin: return 1;
		case FunctionShaderVariable::Pointer: return 1;
		case FunctionShaderVariable::CameraSnapshot: return 1;
		case FunctionShaderVariable::ObjectProperty: return 2;
		case FunctionShaderVariable::VectorNormalize: return 4;
		}
		return 0;
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
		else if (func == FunctionShaderVariable::ObjectProperty)
			return ret == ShaderVariable::ValueType::Float4 || ret == ShaderVariable::ValueType::Float3;
		return false;
	}
	float* FunctionVariableManager::LoadFloat(char* data, int index)
	{
		return (float*)(data + index * sizeof(float));
	}
}
