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
			m_selectedGroup = 0;
			memset(m_owner, 0, PIPELINE_ITEM_NAME_LENGTH * sizeof(char));
		}

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

		void SetOwner(const char* shaderPass);
		void SetType(PipelineItem::ItemType type);
		bool Create();

	private:
		std::vector<std::string> m_groups;
		int m_selectedGroup;

		char m_owner[PIPELINE_ITEM_NAME_LENGTH];
		PipelineItem m_item;
	};
}