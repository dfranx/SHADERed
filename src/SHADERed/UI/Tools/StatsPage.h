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

	private:
		SPIRVParser m_info;
		TextEditor m_spirv;

		std::vector<unsigned int> m_spv;
	};
}