#pragma once
#include <SHADERed/Objects/PipelineItem.h>
#include <glm/glm.hpp>

namespace ed {
	class DebuggerSuggestion {
	public:
		enum class SuggestionType {
			ComputeShader
		};

		DebuggerSuggestion()
		{
			Thread = WorkgroupSize = glm::ivec3(0);
			Type = SuggestionType::ComputeShader;
			Item = nullptr;
		}

		SuggestionType Type;
		PipelineItem* Item;

		glm::ivec3 Thread;
		glm::ivec3 WorkgroupSize;
	};
}