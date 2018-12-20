#pragma once
#include "UIView.h"
#include "VariableValueEdit.h"

namespace ed
{
	class PipelineUI : public UIView
	{
	public:
		using UIView::UIView;

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

	private:
		// for popups
		bool m_isLayoutOpened;
		bool m_isVarManagerOpened;
		ed::PipelineManager::Item* m_modalItem; // item that we are editing in a popup modal
		void m_closePopup();

		// for variable value editor
		ed::VariableValueEditUI m_valueEdit;

		// various small components
		void m_renderItemUpDown(std::vector<ed::PipelineManager::Item*>& items, int index);
		void m_renderItemContext(std::vector<ed::PipelineManager::Item*>& items, int index);
		void m_renderInputLayoutUI();
		void m_renderVariableManagerUI();

		// adding items to pipeline UI
		void m_addShader(const ed::PipelineManager::Item* data);
		void m_addItem(const std::string& name);
	};
}