#pragma once
#include "UIView.h"
#include "../Objects/PipelineItem.h"

namespace ed
{
	class CreateItemUI : public UIView
	{
	public:
		CreateItemUI(GUIManager* ui, InterfaceManager* objects, const std::string& name = "", bool visible = false) : UIView(ui, objects, name, visible) {
			SetOwner(nullptr);
			m_item.Data = nullptr;
		}

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

		void SetOwner(const char* shaderPass);
		void SetType(PipelineItem::ItemType type);
		bool Create();

	private:
		void m_displayBlendOpCombo(const char* name, D3D11_BLEND_OP& op);
		void m_displayBlendCombo(const char* name, D3D11_BLEND& blend);

		char m_owner[PIPELINE_ITEM_NAME_LENGTH];
		PipelineItem m_item;
	};
}