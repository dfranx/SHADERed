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
		
		void Open(ed::PipelineManager::Item* item);
		inline std::string CurrentItemName() { return m_current->Name; }

	private:
		void m_createInputFloat3(const char* label, DirectX::XMFLOAT3& flt);

		ed::PipelineManager::Item* m_current;
	};
}