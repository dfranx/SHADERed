#pragma once
#include "UIView.h"
#include "../ImGUI/TextEditor.h"
#include <deque>

namespace ed
{
	class CodeEditorUI : public UIView
	{
	public:
		CodeEditorUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = false) : UIView(ui, objects, name, visible), m_active(0) {}

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

		void Open(const std::string& str);

	private:
		int m_active;
		std::vector<std::string> m_files;
		std::vector<TextEditor> m_editor;
		std::deque<bool> m_editorOpen;
	};
}