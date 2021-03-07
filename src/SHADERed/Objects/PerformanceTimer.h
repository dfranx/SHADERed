#pragma once
#include <SHADERed/Objects/PipelineItem.h>

namespace ed {
	class PerformanceTimer {
	public:
		PerformanceTimer(PipelineItem* pass) {
			IsDone = true;
			IsCreated = false;
			Object = 0;
			LastTime = 0;
			Pass = pass;
		}

		bool IsCreated;
		bool IsDone;
		unsigned long long LastTime; // Last timer results
		PipelineItem* Pass;

		unsigned int Object; // OpenGL Query
	};
}