#pragma once
#include "PipelineManager.h"
#include <MoonLight/Base/RenderTexture.h>

namespace ed
{
	class RenderEngine
	{
	public:
		RenderEngine(ml::Window* wnd, PipelineManager* pipeline);
		void Render(int width, int height);
	
	private:
		PipelineManager* m_pipeline;
		ml::Window* m_wnd;

		DirectX::XMINT2 m_lastSize;
		ml::RenderTexture m_rt;
	};
}