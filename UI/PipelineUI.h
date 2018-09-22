#pragma once
#include "UIView.h"

namespace ed
{
	class PipelineUI : public UIView
	{
	public:
		using UIView::UIView;

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

	private:
		void m_renderUpDown(std::vector<ed::PipelineManager::Item>& items, int index);
		void m_renderContext(std::vector<ed::PipelineManager::Item>& items, int index);

		void m_addShader(const std::string& name, ed::pipe::ShaderItem* data);
		void m_addGeometry(const std::string& name);
	};
}