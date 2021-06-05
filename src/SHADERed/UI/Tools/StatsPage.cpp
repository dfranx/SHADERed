#include <SHADERed/UI/Tools/StatsPage.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/Settings.h>
#include <imgui/imgui.h>
#include <sstream>

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
		
		if (m_data->Debugger.IsDebugging())
			Highlight(m_data->Debugger.GetCurrentLine()); // TODO: this is copying a vector (although small, but still..) every frame == bad :(

		m_spirv.Render("stats");
		ImGui::Separator();					
	}

	void StatsPage::Refresh(PipelineItem* item, ShaderStage stage)
	{
		m_spv.clear();
		if (item == nullptr)
			return;

		std::string disassembly = "";

		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* pass = (pipe::ShaderPass*)item->Data;

			m_spv = pass->VSSPV;
			if (stage == ShaderStage::Pixel)
				m_spv = pass->PSSPV;
			else if (stage == ShaderStage::Geometry)
				m_spv = pass->GSSPV;
			else if (stage == ShaderStage::TessellationControl)
				m_spv = pass->TCSSPV;
			else if (stage == ShaderStage::TessellationEvaluation)
				m_spv = pass->TESSPV;
		}
		else if (item->Type == PipelineItem::ItemType::ComputePass) {
			pipe::ComputePass* pass = (pipe::ComputePass*)item->Data;

			m_spv = pass->SPV;
		} else if (item->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* pass = (pipe::PluginItemData*)item->Data;

			unsigned int spvSize = pass->Owner->PipelineItem_GetSPIRVSize(pass->Type, pass->PluginData, (ed::plugin::ShaderStage)stage);
			unsigned int* spv = pass->Owner->PipelineItem_GetSPIRV(pass->Type, pass->PluginData, (ed::plugin::ShaderStage)stage); 

			m_spv = std::vector<unsigned int>(spv, spv + spvSize);
		}

		if (!m_spv.empty()) {
			bool res = ShaderCompiler::DisassembleSPIRV(m_spv, disassembly);

			if (res)
				m_parse(disassembly);

			m_info.Parse(m_spv);
		}

		m_spirv.SetPalette(ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme));
		m_spirv.SetLanguageDefinition(TextEditor::LanguageDefinition::SPIRV());
		m_spirv.SetText(disassembly);
	}
	void StatsPage::Highlight(int line)
	{
		if (m_lineMap.count(line))
			m_spirv.SetHighlightedLines(m_lineMap[line]);
		else
			m_spirv.ClearHighlightedLines();
	}
	void StatsPage::m_parse(const std::string& spv)
	{
		m_lineMap.clear();

		std::istringstream iss(spv);
		std::string line = "";

		int curLine = -1;
		int spvLine = 0;

		while (std::getline(iss, line)) {
			if (line.find("OpLine") != std::string::npos) {
				std::istringstream lineSS(line);
				std::vector<std::string> tokens { std::istream_iterator<std::string> { lineSS }, std::istream_iterator<std::string> {} };
				
				if (tokens.size() >= 3)
					curLine = std::stoi(tokens[2]);

				m_lineMap[curLine].push_back(spvLine);
			} else if (curLine != -1) {
				if (line.find("OpFunctionEnd") != std::string::npos)
					curLine = -1;
				else
					m_lineMap[curLine].push_back(spvLine);
			}

			spvLine++;
		}
	}
}