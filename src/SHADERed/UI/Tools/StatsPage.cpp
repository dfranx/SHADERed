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
		ImGui::Text("Arithmetic instruction count: %d", m_info.ArithmeticInstCount);
		ImGui::Text("Bit instruction count: %d", m_info.BitInstCount);
		ImGui::Text("Logical instruction count: %d", m_info.LogicalInstCount);
		ImGui::Text("Texture instruction count: %d", m_info.TextureInstCount);
		ImGui::Text("Derivative instruction count: %d", m_info.DerivativeInstCount);
		ImGui::Text("Control flow instruction count: %d", m_info.ControlFlowInstCount);

		ImGui::NewLine();

		ImGui::Text("SPIR-V: ");
		ImGui::Separator();
		m_spirv.Render("stats");
		ImGui::Separator();					
	}

	void StatsPage::Refresh(PipelineItem* item, ShaderStage stage)
	{
		std::vector<unsigned int>* spv = nullptr;
		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* pass = (pipe::ShaderPass*)item->Data;

			spvtools::SpirvTools core(SPV_ENV_UNIVERSAL_1_3);
			std::string disassembly;

			spv = &pass->VSSPV;
			if (stage == ShaderStage::Pixel)
				spv = &pass->PSSPV;
			else if (stage == ShaderStage::Geometry)
				spv = &pass->GSSPV;

			core.Disassemble(*spv, &disassembly, SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);

			m_spirv.SetPalette(ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme));
			m_spirv.SetText(disassembly);
		}
		else if (item->Type == PipelineItem::ItemType::ComputePass) {
			pipe::ComputePass* pass = (pipe::ComputePass*)item->Data;

			spvtools::SpirvTools core(SPV_ENV_UNIVERSAL_1_3);
			std::string disassembly;

			spv = &pass->SPV;
			core.Disassemble(*spv, &disassembly, SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);

			m_spirv.SetPalette(ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme));
			m_spirv.SetText(disassembly);
		}

		if (spv != nullptr)
			m_info.Parse(*spv);
	}
}