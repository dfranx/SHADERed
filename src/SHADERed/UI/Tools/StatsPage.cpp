#include <SHADERed/UI/Tools/StatsPage.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <SHADERed/Objects/Settings.h>
#include <imgui/imgui.h>
#include <spirv-tools/libspirv.h>
#include <spirv-tools/optimizer.hpp>

namespace ed {
	void StatsPage::OnEvent(const SDL_Event& e) { }
	void StatsPage::Update(float delta)
	{
		ImGui::Text("SPIR-V: ");
		ImGui::Separator();
		m_spirv.Render("stats");
		ImGui::Separator();					
	}

	void StatsPage::Refresh(PipelineItem* item, ShaderStage stage)
	{
		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* pass = (pipe::ShaderPass*)item->Data;

			spvtools::SpirvTools core(SPV_ENV_UNIVERSAL_1_3);
			std::string disassembly;
			
			if (stage == ShaderStage::Vertex)
				core.Disassemble(pass->VSSPV, &disassembly, SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);
			else if (stage == ShaderStage::Pixel)
				core.Disassemble(pass->PSSPV, &disassembly, SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);
			else if (stage == ShaderStage::Geometry)
				core.Disassemble(pass->GSSPV, &disassembly, SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);

			m_spirv.SetPalette(ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme));
			m_spirv.SetText(disassembly);
		}
		// TODO: compute shaders
	}
}