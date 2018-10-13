#pragma once
#include "PipelineManager.h"
#include <MoonLight/Base/RenderTexture.h>
#include <MoonLight/Base/ShaderResourceView.h>
#include <MoonLight/Base/Timer.h>

#include <unordered_map>

namespace ed
{
	class RenderEngine
	{
	public:
		RenderEngine(ml::Window* wnd, PipelineManager* pipeline);
		~RenderEngine();

		void Render(int width, int height);
		void Recompile(const char* name);

		inline ml::ShaderResourceView& GetTexture() { return m_rtView; }
	
	private:
		PipelineManager* m_pipeline;
		ml::Window* m_wnd;

		DirectX::XMINT2 m_lastSize;
		ml::RenderTexture m_rt;

		ml::ShaderResourceView m_rtView;

		std::vector<ed::PipelineManager::Item> m_items;

		std::vector<ed::PipelineManager::Item> m_cachedItems;
		std::vector<void*> m_d3dItems;

		inline bool m_isCached(const ed::PipelineManager::Item& item) { return item.Type == ed::PipelineItem::ShaderFile; }

		ml::Timer m_cacheTimer;
		void m_cache();
	};
}