#pragma once
#include "ShaderVariable.h"

namespace ed
{
	class FunctionVariableManager
	{
	public:
		static size_t GetArgumentCount(ed::FunctionShaderVariable func);
		static void AllocateArgumentSpace(ed::ShaderVariable& var, ed::FunctionShaderVariable func);
		static bool HasValidReturnType(ShaderVariable::ValueType ret, ed::FunctionShaderVariable func);
		static void Update(ed::ShaderVariable& var);
		static float* LoadFloat(char* data, int index);
	};
}