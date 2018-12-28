#pragma once
#include "UIView.h"

namespace ed
{
	class PropertyUI : public UIView
	{
	public:
		PropertyUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true) : UIView(ui, objects, name, visible), m_current(nullptr) {}

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);
		
		void Open(ed::PipelineItem* item);
		inline std::string CurrentItemName() { return m_current->Name; }
		inline bool HasItemSelected() { return m_current != nullptr; }

	private:
		char m_itemName[64];

		ed::PipelineItem* m_current;
	};
}