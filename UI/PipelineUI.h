#pragma once
#include "UIView.h"
#include "VariableValueEdit.h"
#include "CreateItemUI.h"

namespace ed
{
	class PipelineUI : public UIView
	{
	public:
		PipelineUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = false) :
			UIView(ui, objects, name, visible), 
			m_createUI(ui, objects) {
		}

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

	private:
		// for popups
		bool m_isLayoutOpened;
		bool m_isVarManagerOpened;
		bool m_isCreateViewOpened;
		bool m_isVarManagerForVS; // do we edit the variables for vertex or pixel shader? (in shader pass)

		std::vector<pipe::ShaderPass*> m_expandList; // list of shader pass items that are collapsed (we use pointers to identify a specific items - i know its bad but too late rn [TODO])

		CreateItemUI m_createUI;
		ed::PipelineItem* m_modalItem; // item that we are editing in a popup modal
		void m_closePopup();

		// for variable value editor
		ed::VariableValueEditUI m_valueEdit;

		// various small components
		void m_renderItemUpDown(std::vector<ed::PipelineItem*>& items, int index);
		bool m_renderItemContext(std::vector<ed::PipelineItem*>& items, int index);
		void m_renderInputLayoutUI();
		void m_renderVariableManagerUI();

		// adding items to pipeline UI
		void m_addShaderPass(const ed::PipelineItem* data);
		void m_addItem(const std::string& name);
	};
}