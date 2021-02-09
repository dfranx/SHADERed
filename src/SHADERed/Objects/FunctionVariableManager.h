#pragma once
#include <SHADERed/Objects/ShaderVariable.h>
#include <SHADERed/Objects/PipelineManager.h>
#include <SHADERed/Objects/Debug/PixelInformation.h>
#include <SHADERed/Engine/Timer.h>
#include <vector>

namespace ed {
	class FunctionVariableManager {
	public:
		FunctionVariableManager();

		void Initialize(PipelineManager* pipe, DebugInformation* dbgr, RenderEngine* rndr);

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
		std::unordered_map<void*, eng::Timer> m_vertexShaderTimer;
		std::unordered_map<void*, glm::vec4> m_vertexShaderCache;

		int m_currentIndex;
		PipelineManager* m_pipeline;
		DebugInformation* m_debugger;
		RenderEngine* m_renderer;
	};
}