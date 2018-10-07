#pragma once
#include "PipelineManager.h"
#include <MoonLight/Base/RenderTexture.h>
#include <MoonLight/Base/ShaderResourceView.h>

#include <unordered_map>

namespace ed
{
	class RenderEngine
	{
	public:
		RenderEngine(ml::Window* wnd, PipelineManager* pipeline);
		void Render(int width, int height);

		inline ml::ShaderResourceView& GetTexture() { return m_rtView; }
	
	private:
		PipelineManager* m_pipeline;
		ml::Window* m_wnd;

		DirectX::XMINT2 m_lastSize;
		ml::RenderTexture m_rt;

		ml::ShaderResourceView m_rtView;
	};
}