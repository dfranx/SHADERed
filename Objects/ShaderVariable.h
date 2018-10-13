#pragma once
#include "../Options.h"
#include <string>

namespace ed
{
	enum class SystemShaderVariable
	{
		None,				// not using system value
		Time,				// float	- time elapsed since start
		TimeDelta,			// float	- render time
		ViewportSize,		// float2	- rendering window size
		MousePosition,		// float2	- mouse position relative to (0,0) in the rendering window
		View,				// matrix/float4x4 - a built-in camera matrix
		Projection,			// matrix/float4x4 - a built-in projection matrix
		ViewProjection,		// matrix/float4x4 - View*Projection matrix
	};

	enum class FunctionShaderVariable
	{
		None,					// using user input instead of built-in functions
		MatrixIdentity,			// XMMatrixIdentity()
		MatrixLookAtLH,			// XMMatrixLookAtLH(float3 EyePosition, float3 FocusPosition, float3 UpDirection)
		MatrixLookToLH,			// XMMatrixLookToLH(float3 EyePosition, float3 EyeDirection, float3 UpDirection)
		MatrixOrthographicLH,	// XMMatrixOrthographicLH(float ViewWidth, float ViewHeight, float NearZ, float FarZ)
		MatrixPerspectiveFovLH,	// XMMatrixPerspectiveFovLH(float fovAngleY, float AspectRatio, float NearZ, float FarZ)
		MatrixPerspectiveLH,	// XMMatrixPerspectiveLH(float ViewWidth, float ViewHeight, float NearZ, float FarZ)
		MatrixReflect,			// XMMatrixReflect(float4 ReflectionPlane)
		MatrixRotationAxis,		// XMMatrixRotationAxis(float4 axis, float angle)
		MatrixRotationNormal,	// XMMatrixRotationNormal(float4 normal, float angle)
		MatrixRotationRollPitchYaw, // XMMatrixRotationRollPitchYaw(float pitch, float yaw, float roll)
		MatrixRotationX,		// XMMatrixRotationX(float angle)
		MatrixRotationY,		// XMMatrixRotationY(float angle)
		MatrixRotationZ,		// XMMatrixRotationZ(float angle)
		MatrixScaling,			// XMMatrixScaling(float x, float y, float z)
		MatrixShadow,			// XMMatrixShadow(float4 ShadowPlane, float4 LightPosition) -> 4th param actually bool, 0 = directional light, 1 = point light
		MatrixTranslation,		// XMMatrixScaling(float offsetX, float offsetY, float offsetZ)
		ScalarCos,				// XMScalarCos(float radians)
		ScalarSin,				// XMScalarSin(float radians)
		VectorNormalize,		// XMVector2|3|4Normalize()
	};

	class ShaderVariable
	{
	public:
		enum class ValueType
		{
			Boolean1,
			Boolean2,
			Boolean3,
			Boolean4,
			Integer1,
			Integer2,
			Integer3,
			Integer4,
			Float1,
			Float2,
			Float3,
			Float4,
			Float2x2,
			Float3x3,
			Float4x4
		};

		ShaderVariable(ValueType type, const char* name = "var\0", SystemShaderVariable systemVar = SystemShaderVariable::None, int slot = 0) :
			m_type(type), System(systemVar), Slot(slot)
		{
			Data = (char*)calloc(GetSize(type), 1);
			memset(Name, 0, VARIABLE_NAME_LENGTH);
			memcpy(Name, name, strlen(name));
		}

		static inline int GetSize(ValueType type)
		{
			switch (type) {
			case ValueType::Boolean1: return 1 * 4;
			case ValueType::Boolean2: return 2 * 4;
			case ValueType::Boolean3: return 3 * 4;
			case ValueType::Boolean4: return 4 * 4;

			case ValueType::Integer1: return 1 * 4;
			case ValueType::Integer2: return 2 * 4;
			case ValueType::Integer3: return 3 * 4;
			case ValueType::Integer4: return 4 * 4;

			case ValueType::Float1: return 1 * 4;
			case ValueType::Float2: return 2 * 4;
			case ValueType::Float3: return 3 * 4;
			case ValueType::Float4: return 4 * 4;

			case ValueType::Float2x2: return 2*2 * 4;
			case ValueType::Float3x3: return 3*3 * 4;
			case ValueType::Float4x4: return 4*4 * 4;
			}
			return 0;
		}
		
		char Name[VARIABLE_NAME_LENGTH];	// just a descriptive name to help you out
		SystemShaderVariable System;		// do we provide the value or does our system provide the value?
		int Slot;			// b0, b1, ..., b15
		char* Data;			// allocated with malloc()

		inline int AsInteger(int index = 0) { return *(int*)(Data + index * GetSize(ValueType::Integer1)); }
		inline bool AsBoolean(int index = 0) { return *(bool*)(Data + index * GetSize(ValueType::Boolean1)); }
		inline float AsFloat(int col = 0, int row = 0) { return *(bool*)(Data + (row*GetColumnCount() + col) * GetSize(ValueType::Float1)); }

		inline int* AsIntegerPtr(int index = 0) { return (int*)(Data + index * GetSize(ValueType::Integer1)); }
		inline bool* AsBooleanPtr(int index = 0) { return (bool*)(Data + index * GetSize(ValueType::Boolean1)); }
		inline float* AsFloatPtr(int col = 0, int row = 0) { return (float*)(Data + (row*GetColumnCount() + col) * GetSize(ValueType::Float1)); }

		inline void SetIntegerValue(int value, int index = 0) { *AsIntegerPtr(index) = value; }
		inline void SetBooleanValue(bool value, int index = 0) { *AsBooleanPtr(index) = value; }
		inline void SetFloat(float value, int col = 0, int row = 0) { *AsFloatPtr(col, row) = value; }

		inline ValueType GetType() { return m_type; }
		inline int GetColumnCount()
		{
			switch (m_type) {
				case ValueType::Float2x2: return 2;
				case ValueType::Float3x3: return 3;
				case ValueType::Float4x4: return 4;
			}
			return 0;
		}

	private:
		ValueType m_type;	// the size and type of this variable - protect this value, it can only be set at the variable defintion - cant be changed
	};
}