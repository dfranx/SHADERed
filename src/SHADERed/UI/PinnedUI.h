#pragma once
#include <SHADERed/UI/Tools/VariableValueEdit.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class PinnedUI : public UIView {
	public:
		PinnedUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
				, m_editor(objects)
		{
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		inline void CloseAll() { m_pinnedVars.clear(); }
		inline std::vector<ed::ShaderVariable*>& GetAll() { return m_pinnedVars; }

		void Add(ed::ShaderVariable* var);
		void Remove(const char* name);
		bool Contains(const char* name);

	private:
		std::vector<ed::ShaderVariable*> m_pinnedVars;
		VariableValueEditUI m_editor;
	};
}