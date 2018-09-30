#include "RenderEngine.h"

namespace ed
{
	RenderEngine::RenderEngine(ml::Window* wnd, PipelineManager * pipeline) :
		m_pipeline(pipeline),
		m_wnd(wnd),
		m_lastSize(0, 0)
	{}
	void RenderEngine::Render(int width, int height)
	{
		if (m_lastSize.x != width || m_lastSize.y != height) {
			m_lastSize = DirectX::XMINT2(width, height);
			m_rt.Create(*m_wnd, m_lastSize, ml::Resource::ShaderResource);
		}

		std::vector<ed::PipelineManager::Item>& items = m_pipeline->GetList();

		m_rt.Bind();
		m_rt.Clear();
		m_wnd->ClearDepthStencil(1.0f, 0);
		
		for (auto it : items) {
			// TODO: cache shaders, input layouts, etc..
		}
	}
}