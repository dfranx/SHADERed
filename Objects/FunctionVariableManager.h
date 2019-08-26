#pragma once
#include "ShaderVariable.h"
#include <vector>

namespace ed
{
	class FunctionVariableManager
	{
	public:
		static size_t GetArgumentCount(ed::FunctionShaderVariable func);
		static void AllocateArgumentSpace(ed::ShaderVariable* var, ed::FunctionShaderVariable func);
		static bool HasValidReturnType(ShaderVariable::ValueType ret, ed::FunctionShaderVariable func);
		static void AddToList(ed::ShaderVariable* var);
		static void Update(ed::ShaderVariable* var);
		static float* LoadFloat(char* data, int index);

		static void ClearVariableList();

		static int CurrentIndex;
		static std::vector<ed::ShaderVariable*> VariableList;
	};
}