#pragma once
#include <SHADERed/Objects/PluginAPI/Plugin.h>
#include <SHADERed/Options.h>
#include <string.h>
#include <string>

namespace ed {
	struct PluginSystemVariableData {
		char Name[64];
		IPlugin1* Owner;
	};

	struct PluginFunctionData {
		char Name[64];
		IPlugin1* Owner;
	};

	// TODO: add these to TUTORIAL.md / wiki
	enum class SystemShaderVariable {
		None,			   // not using system value
		Time,			   // float - time elapsed since start
		TimeDelta,		   // float - render time
		FrameIndex,		   // uint	 - current frame index
		ViewportSize,	   // vec2 - rendering window size
		MousePosition,	   // vec2 - mouse position relative to (0,0) in the rendering window
		View,			   // mat4 - a built-in camera matrix
		Projection,		   // mat4 - a built-in projection matrix
		ViewProjection,	   // mat4 - View*Projection matrix
		Orthographic,	   // mat4 - an orthographic matrix
		ViewOrthographic,  // mat4 - View*Orthographic
		GeometryTransform, // mat4 - apply Scale, Rotation and Position to geometry
		IsPicked,		   // bool - check if current item is selected
		IsSavingToFile,    // bool - are we currently rendering to a preview window or to an image file?
		CameraPosition,	   // vec4 - current camera position
		CameraPosition3,   // vec3 - current camera position
		CameraDirection3,  // vec3 - camera view direction
		KeysWASD,		   // vec4 - are W, A, S or D keys pressed
		Mouse,			   // vec4 - (x,y,left,right) updated every frame
		MouseButton,	   // vec4 - (x,y,left,right) updated only when mouse button pressed
		PickPosition,	   // vec3 - last pick position
		PluginVariable,	   // a value that is updated by some plugin
		Count
	};

	enum class FunctionShaderVariable {
		None, // using user input instead of built-in functions
		Pointer,
		CameraSnapshot,
		ObjectProperty,
		MatrixIdentity,
		MatrixLookAtLH,
		MatrixLookToLH,
		MatrixOrthographicLH,
		MatrixPerspectiveFovLH,
		MatrixPerspectiveLH,
		MatrixRotationAxis,
		MatrixRotationNormal,
		MatrixRotationRollPitchYaw,
		MatrixRotationX,
		MatrixRotationY,
		MatrixRotationZ,
		MatrixScaling,
		MatrixTranslation,
		ScalarCos,
		ScalarSin,
		VectorNormalize,
		PluginFunction, // a function that is executed by the plugin
		Count			// not usable - this value just tells us number of all functions
	};

	class ShaderVariable {
	public:
		enum class ValueType {
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
			Float4x4,
			Count
		};
		enum class Flag {
			None = 0b00000000,		// no flags
			LastFrame = 0b00000001, // use previous value instead of the current value
			Inverse = 0b00000010	// inverse(matrix)
		};

		ShaderVariable(ValueType type, const char* name = "var\0", SystemShaderVariable systemVar = SystemShaderVariable::None)
				: m_type(type)
				, System(systemVar)
		{
			Arguments = nullptr;
			Data = (char*)calloc(GetSize(type), 1);
			memset(Name, 0, VARIABLE_NAME_LENGTH);
			memcpy(Name, name, strlen(name));
			Function = FunctionShaderVariable::None;
			Flags = 0;
		}

		static inline int GetSize(ValueType type, bool isBuffer = false)
		{
			switch (type) {
			case ValueType::Boolean1: return 1 * (isBuffer ? 1 : 4);
			case ValueType::Boolean2: return 2 * (isBuffer ? 1 : 4);
			case ValueType::Boolean3: return 3 * (isBuffer ? 1 : 4);
			case ValueType::Boolean4: return 4 * (isBuffer ? 1 : 4);

			case ValueType::Integer1: return 1 * 4;
			case ValueType::Integer2: return 2 * 4;
			case ValueType::Integer3: return 3 * 4;
			case ValueType::Integer4: return 4 * 4;

			case ValueType::Float1: return 1 * 4;
			case ValueType::Float2: return 2 * 4;
			case ValueType::Float3: return 3 * 4;
			case ValueType::Float4: return 4 * 4;

			case ValueType::Float2x2: return 2 * 2 * 4;
			case ValueType::Float3x3: return 3 * 3 * 4;
			case ValueType::Float4x4: return 4 * 4 * 4;
			}
			return 0;
		}

		char Name[VARIABLE_NAME_LENGTH]; // name that a uniform variable has
		SystemShaderVariable System;	 // do we provide the value or does our system provide the value?
		FunctionShaderVariable Function; // do we input value or does system calculate it for us?
		char* Data;						 // allocated with malloc()
		char* Arguments;				 // space to store arguments for function - allocated if not null!!!
		char Flags;
		PluginSystemVariableData PluginSystemVarData;
		PluginFunctionData PluginFuncData;

		inline int AsInteger(int index = 0) { return *AsIntegerPtr(index); }
		inline bool AsBoolean(int index = 0) { return *AsBooleanPtr(index); }
		inline float AsFloat(int col = 0, int row = 0) { return *AsFloatPtr(col, row); }

		inline int* AsIntegerPtr(int index = 0) { return (int*)(Data + index * GetSize(ValueType::Integer1)); }
		inline bool* AsBooleanPtr(int index = 0) { return (bool*)(Data + index * GetSize(ValueType::Boolean1)); }

		inline float* AsFloatPtr(int col = 0, int row = 0) { return (float*)(Data + (row * GetColumnCount() + col) * GetSize(ValueType::Float1)); }

		inline void SetIntegerValue(int value, int index = 0) { *AsIntegerPtr(index) = value; }
		inline void SetBooleanValue(bool value, int index = 0) { *AsBooleanPtr(index) = value; }
		inline void SetFloat(float value, int col = 0, int row = 0) { *AsFloatPtr(col, row) = value; }

		inline void SetType(ValueType newType)
		{
			System = SystemShaderVariable::None;
			Function = FunctionShaderVariable::None;
			Flags = (char)Flag::None;

			if (Arguments != nullptr) {
				free(Arguments);
				Arguments = nullptr;
			}

			m_type = newType;
			char* tempData = (char*)realloc(Data, ShaderVariable::GetSize(m_type));
			if (tempData != nullptr)
				Data = tempData;
		}
		inline ValueType GetType() { return m_type; }
		inline int GetColumnCount()
		{
			switch (m_type) {
			case ValueType::Boolean1: return 1;
			case ValueType::Boolean2: return 2;
			case ValueType::Boolean3: return 3;
			case ValueType::Boolean4: return 4;
			case ValueType::Integer1: return 1;
			case ValueType::Integer2: return 2;
			case ValueType::Integer3: return 3;
			case ValueType::Integer4: return 4;
			case ValueType::Float1: return 1;
			case ValueType::Float2: return 2;
			case ValueType::Float3: return 3;
			case ValueType::Float4: return 4;
			case ValueType::Float2x2: return 2;
			case ValueType::Float3x3: return 3;
			case ValueType::Float4x4: return 4;
			}

			return 0;
		}
		inline int GetRowCount()
		{
			switch (m_type) {
			case ValueType::Float2x2: return 2;
			case ValueType::Float3x3: return 3;
			case ValueType::Float4x4: return 4;
			}

			return 1;
		}
		inline ValueType GetBaseType()
		{
			switch (m_type) {
			case ValueType::Boolean1: return ValueType::Boolean1;
			case ValueType::Boolean2: return ValueType::Boolean1;
			case ValueType::Boolean3: return ValueType::Boolean1;
			case ValueType::Boolean4: return ValueType::Boolean1;
			case ValueType::Integer1: return ValueType::Integer1;
			case ValueType::Integer2: return ValueType::Integer1;
			case ValueType::Integer3: return ValueType::Integer1;
			case ValueType::Integer4: return ValueType::Integer1;
			case ValueType::Float1: return ValueType::Float1;
			case ValueType::Float2: return ValueType::Float1;
			case ValueType::Float3: return ValueType::Float1;
			case ValueType::Float4: return ValueType::Float1;
			case ValueType::Float2x2: return ValueType::Float1;
			case ValueType::Float3x3: return ValueType::Float1;
			case ValueType::Float4x4: return ValueType::Float1;
			}

			return ValueType::Float1;
		}

	private:
		ValueType m_type; // the size and type of this variable - protect this value, it can only be set at the variable defintion - cant be changed
	};
}