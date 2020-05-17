#pragma once
#include <SHADERed/Engine/Model.h>
#include <SHADERed/Objects/PipelineItem.h>

#include <glm/glm.hpp>
#include <unordered_map>

namespace ed {
	namespace dbg {
		class Breakpoint {
		public:
			Breakpoint() { Line = 0; IsConditional = false; Condition = ""; }
			Breakpoint(int ln) { Line = ln; IsConditional = false; Condition = ""; }
			Breakpoint(int ln, const std::string& cond) { Line = ln; IsConditional = true; Condition = cond; }

			int Line;
			bool IsConditional;
			std::string Condition;
		};
	}
}