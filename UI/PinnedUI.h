#pragma once
#include "UIView.h"
#include "VariableValueEdit.h"

namespace ed
{
	class PinnedUI : public UIView
	{
	public:
		using UIView::UIView;

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

		void Add(ed::ShaderVariable* var);
		void Remove(const char* name);
		bool Contains(const char* name);

	private:
		std::vector<ed::ShaderVariable*> m_pinnedVars;
		VariableValueEditUI m_editor;
	};
}