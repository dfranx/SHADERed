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
			UIView(ui, objects, name, visible), m_isChangeVarsOpened(false),
			m_isCreateViewOpened(false), m_isVarManagerOpened(false), m_modalItem(nullptr),
			m_createUI(ui, objects) {
			m_itemMenuOpened = false;
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);
		
		inline void Reset() { m_expandList.clear(); }

		inline std::vector<pipe::ShaderPass*>& GetCollapsedItems() { return m_expandList; }
		inline void Collapse(pipe::ShaderPass* data) { m_expandList.push_back(data); }

	private:
		// for popups
		bool m_isVarManagerOpened;
		bool m_isChangeVarsOpened;
		bool m_isCreateViewOpened;
		bool m_itemMenuOpened;

		std::vector<pipe::ShaderPass*> m_expandList; // list of shader pass items that are collapsed

		CreateItemUI m_createUI;
		ed::PipelineItem* m_modalItem; // item that we are editing in a popup modal
		void m_closePopup();

		// for variable value editor
		ed::VariableValueEditUI m_valueEdit;

		// various small components
		void m_renderItemUpDown(std::vector<ed::PipelineItem*>& items, int index);
		bool m_renderItemContext(std::vector<ed::PipelineItem*>& items, int index);
		void m_renderVariableManagerUI();
		void m_renderChangeVariablesUI();

		// adding items to pipeline UI
		void m_addShaderPass(ed::PipelineItem* data);
		void m_addItem(ed::PipelineItem* name);
	};
}