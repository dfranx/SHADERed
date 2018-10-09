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
		bool m_isLayoutOpened;
		bool m_isVarManagerOpened;

		ed::PipelineManager::Item* m_modalItem; // item that we are editing in a popup modal

		void m_closePopup();

		void m_renderItemUpDown(std::vector<ed::PipelineManager::Item>& items, int index);
		void m_renderItemContext(std::vector<ed::PipelineManager::Item>& items, int index);
		void m_renderInputLayoutUI();
		void m_renderVariableManagerUI();

		void m_addShader(const std::string& name, ed::pipe::ShaderItem* data);
		void m_addItem(const std::string& name);
	};
}