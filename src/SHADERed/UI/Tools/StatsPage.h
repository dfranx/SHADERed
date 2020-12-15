#pragma once
#include <SHADERed/InterfaceManager.h>
#include <SHADERed/UI/UIView.h>
#include <SHADERed/Objects/SPIRVParser.h>
#include <ImGuiColorTextEdit/TextEditor.h>

namespace ed {
	class StatsPage : public UIView {
	public:
		StatsPage(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
			m_spirv.SetReadOnly(true);
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		void Refresh(PipelineItem* item, ShaderStage stage);

		inline void ClearHighlights() { m_spirv.ClearHighlightedLines(); }
		void Highlight(int line);

	private:
		SPIRVParser m_info;
		TextEditor m_spirv;

		void m_parse(const std::string& spv);

		std::vector<unsigned int> m_spv;
		std::unordered_map<int, std::vector<int>> m_lineMap;
	};
}