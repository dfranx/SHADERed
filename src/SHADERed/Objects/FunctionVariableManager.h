#pragma once
#include <SHADERed/Objects/ShaderVariable.h>
#include <SHADERed/Objects/PipelineManager.h>
#include <vector>

namespace ed {
	class FunctionVariableManager {
	public:
		FunctionVariableManager();

		void Initialize(PipelineManager* pipe);

		void AddToList(ed::ShaderVariable* var);
		void Update(ed::ShaderVariable* var);

		static void AllocateArgumentSpace(ed::ShaderVariable* var, ed::FunctionShaderVariable func);
		static size_t GetArgumentCount(ed::FunctionShaderVariable func);
		static bool HasValidReturnType(ShaderVariable::ValueType ret, ed::FunctionShaderVariable func);
		static float* LoadFloat(char* data, int index);

		void ClearVariableList();

		std::vector<ed::ShaderVariable*> VariableList;

		static inline FunctionVariableManager& Instance()
		{
			static FunctionVariableManager ret;
			return ret;
		}

	private:
		int m_currentIndex;
		PipelineManager* m_pipeline;
	};
}