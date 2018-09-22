#pragma once
#include "PipelineManager.h"

namespace ed
{
	class RenderEngine
	{
	public:
		RenderEngine(PipelineManager* pipeline);
		void Render();
	
	private:
		PipelineManager* m_pipeline;
	};
}