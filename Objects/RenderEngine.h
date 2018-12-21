#pragma once
#include "PipelineManager.h"
#include "ProjectParser.h"
#include "MessageStack.h"

#include <MoonLight/Base/ShaderResourceView.h>
#include <MoonLight/Base/RenderTexture.h>
#include <MoonLight/Base/VertexShader.h>
#include <MoonLight/Base/PixelShader.h>
#include <MoonLight/Base/Timer.h>
#include <unordered_map>

namespace ed
{
	class RenderEngine
	{
	public:
		RenderEngine(ml::Window* wnd, PipelineManager* pipeline, ProjectParser* project, MessageStack* messages);
		~RenderEngine();

		void Render(int width, int height);
		void Recompile(const char* name);

		void FlushCache();

		inline ml::ShaderResourceView& GetTexture() { return m_rtView; }
	
	private:
		PipelineManager* m_pipeline;
		ProjectParser* m_project;
		MessageStack* m_msgs;
		ml::Window* m_wnd;

		DirectX::XMINT2 m_lastSize;
		ml::RenderTexture m_rt;

		ml::ShaderResourceView m_rtView;

		std::vector<ed::PipelineItem*> m_items;
		std::vector<ml::VertexShader*> m_vs;
		std::vector<ml::PixelShader*> m_ps;

		ml::Timer m_cacheTimer;
		void m_cache();
	};
}