#include "CodeEditorUI.h"
#include "../ImGUI/imgui.h"
#include "../Objects/Names.h"

#include <fstream>
#include <d3dcompiler.h>

namespace ed
{
	void CodeEditorUI::OnEvent(const ml::Event & e)
	{
		if (m_selectedItem == -1)
			return;

		if (e.Type == ml::EventType::KeyRelease) {
			if (e.Keyboard.VK == VK_F5) {
				if (e.Keyboard.Control) {
					// ALT+F5 -> switch between stats and code
					if (!m_stats[m_selectedItem].IsActive)
						m_fetchStats(m_selectedItem);
					else m_stats[m_selectedItem].IsActive = false;
				} else {
					// F5 -> compile shader
					m_compile(m_selectedItem);
				}
			} else if (e.Keyboard.VK == 'S') {
				if (e.Keyboard.Control) // CTRL+S -> save file
					m_save(m_selectedItem);
			}
		}
	}
	void CodeEditorUI::Update(float delta)
	{
		if (m_editor.size() == 0)
			return;
		
		m_selectedItem = -1;

		// dock space
		for (int i = 0; i < m_editor.size(); i++) {
			if (m_editorOpen[i]) {
				ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
				if (ImGui::Begin((std::string(m_items[i].Name) + "##code_view").c_str(), &m_editorOpen[i], ImGuiWindowFlags_MenuBar)) {
					
					if (ImGui::BeginMenuBar()) {
						if (ImGui::BeginMenu("File")) {
							if (ImGui::MenuItem("Save", "CTRL+S")) m_save(i);
							ImGui::EndMenu();
						}
						if (ImGui::BeginMenu("Code")) {
							if (ImGui::MenuItem("Compile", "F5")) m_compile(i);
							if (!m_stats[i].IsActive && ImGui::MenuItem("Stats", "CTRL+F5")) m_fetchStats(i);
							if (m_stats[i].IsActive && ImGui::MenuItem("Code", "CTRL+F5")) m_stats[i].IsActive = false;
							ImGui::Separator();
							if (ImGui::MenuItem("Undo", "CTRL+Z", nullptr, m_editor[i].CanUndo())) m_editor[i].Undo();
							if (ImGui::MenuItem("Redo", "CTRL+Y", nullptr, m_editor[i].CanRedo())) m_editor[i].Redo();
							ImGui::Separator();
							if (ImGui::MenuItem("Cut", "CTRL+X")) m_editor[i].Cut();
							if (ImGui::MenuItem("Copy", "CTRL+C")) m_editor[i].Copy();
							if (ImGui::MenuItem("Paste", "CTRL+V")) m_editor[i].Paste();
							if (ImGui::MenuItem("Select All", "CTRL+A")) m_editor[i].SelectAll();

							ImGui::EndMenu();
						}

						ImGui::EndMenuBar();
					}

					if (m_stats[i].IsActive)
						m_renderStats(i);
					else {
						ImGui::PushFont(m_consolas);
						m_editor[i].Render(m_items[i].Name);
						ImGui::PopFont();
					}

					if (m_editor[i].IsFocused())
						m_selectedItem = i;
				}
				ImGui::End();
			}
		}

		// delete not needed editors
		for (int i = 0; i < m_editorOpen.size(); i++) {
			if (!m_editorOpen[i]) {
				m_items.erase(m_items.begin() + i);
				m_editor.erase(m_editor.begin() + i);
				m_editorOpen.erase(m_editorOpen.begin() + i);
				m_stats.erase(m_stats.begin() + i);
				i--;
			}
		}
	}
	void CodeEditorUI::Open(PipelineManager::Item item)
	{
		// check if already opened
		int fileCount = std::count_if(m_items.begin(), m_items.end(), [&](const PipelineManager::Item& a) { return strcmp(a.Name, item.Name) == 0; });
		if (fileCount != 0)
			return;

		ed::pipe::ShaderItem* shader = reinterpret_cast<ed::pipe::ShaderItem*>(item.Data);

		m_items.push_back(item);
		m_editor.push_back(TextEditor());
		m_editorOpen.push_back(true);
		m_stats.push_back(StatsPage());

		TextEditor* editor = &m_editor[m_editor.size() - 1];
		
		TextEditor::LanguageDefinition lang = TextEditor::LanguageDefinition::HLSL();
		TextEditor::Palette pallete = TextEditor::GetDarkPalette();

		pallete[(int)TextEditor::PaletteIndex::Background] = 0x00000000;

		editor->SetPalette(pallete);
		editor->SetLanguageDefinition(lang);

		std::ifstream file(shader->FilePath);
		if (file.is_open()) {
			std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			editor->SetText(str);
			file.close();
		}
	}
	void CodeEditorUI::m_save(int id)
	{
		ed::pipe::ShaderItem* shader = reinterpret_cast<ed::pipe::ShaderItem*>(m_items[id].Data);
		std::ofstream file(shader->FilePath);
		if (file.is_open()) {
			file << m_editor[id].GetText();
			file.close();
		}
	}
	void CodeEditorUI::m_compile(int id)
	{
		m_save(id);
		m_data->Renderer.Recompile(m_items[id].Name);
	}
	void CodeEditorUI::m_fetchStats(int id)
	{
		m_stats[id].IsActive = true;

		pipe::ShaderItem* data = (pipe::ShaderItem*)m_items[id].Data;
		ID3DBlob *bytecodeBlob = nullptr, *errorBlob = nullptr;

		// get shader version
		std::string type = "ps_5_0";
		if (data->Type == pipe::ShaderItem::VertexShader)
			type = "vs_5_0";

		// generate bytecode
		D3DCompile(m_editor[id].GetText().c_str(), m_editor[id].GetText().size(), m_items[id].Name, nullptr, nullptr, data->Entry, type.c_str(), 0, 0, &bytecodeBlob, &errorBlob);
	
		// delete the error data, we dont need it
		if (errorBlob != nullptr) {
			/* TODO: error stack */
			errorBlob->Release();
			errorBlob = nullptr;

			if (bytecodeBlob != nullptr) {
				bytecodeBlob->Release();
				bytecodeBlob = nullptr;
			}

			m_stats[id].IsActive = false;
			return;
		}

		// shader reflection
		D3DReflect(bytecodeBlob->GetBufferPointer(), bytecodeBlob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&m_stats[id].Info);
	}
	void CodeEditorUI::m_renderStats(int id)
	{
		ImGui::PushFont(m_consolas);
		/* ImVec4(1.0f, 0.17f, 0.13f, 1.0f) for errors */

		ID3D11ShaderReflection* shader = (ID3D11ShaderReflection*)m_stats[id].Info;

		D3D11_SHADER_DESC shaderDesc;
		shader->GetDesc(&shaderDesc);
		
		ImGui::TextColored(ImVec4(1.0f, 0.89f, 0.71f, 1.0f), shaderDesc.Creator);
		ImGui::Text("Bound Resource Count: %d", shaderDesc.BoundResources);
		ImGui::Text("Temprorary Register Count: %d", shaderDesc.TempRegisterCount);
		ImGui::Text("Temprorary Array Count: %d", shaderDesc.TempArrayCount);
		ImGui::Text("Number of constant defines: %d", shaderDesc.DefCount);
		ImGui::Text("Declaration Count: %d", shaderDesc.DclCount);

		ImGui::NewLine();
		ImGui::Text("Constant Buffer Count: %d", shaderDesc.ConstantBuffers);
		ImGui::Separator();
		for (int i = 0; i < shaderDesc.ConstantBuffers; i++) {
			ID3D11ShaderReflectionConstantBuffer* cb = shader->GetConstantBufferByIndex(i);
			
			D3D11_SHADER_BUFFER_DESC cbDesc;
			cb->GetDesc(&cbDesc);

			ImGui::Text("%s:", cbDesc.Name);
			ImGui::SameLine();
			ImGui::Indent(160);
				ImGui::Text("Type: %s", (cbDesc.Type == D3D11_CT_CBUFFER) ? "scalar" : "texture");
				ImGui::Text("Size: %dB", cbDesc.Size);
				ImGui::Text("Variable Count: %d", cbDesc.Variables);
				for (int j = 0; j < cbDesc.Variables; j++) {
					ID3D11ShaderReflectionVariable* var = cb->GetVariableByIndex(j);

					D3D11_SHADER_VARIABLE_DESC varDesc;
					var->GetDesc(&varDesc);
					
					ImGui::Text("%s:", varDesc.Name);
					ImGui::SameLine();

					ImGui::Indent(160);
						D3D11_SHADER_TYPE_DESC typeDesc;
						var->GetType()->GetDesc(&typeDesc);
						ImGui::Text("Type: %s", typeDesc.Name);
						ImGui::Text("Size: %dB", varDesc.Size);
						ImGui::Text("Offset: %d", varDesc.StartOffset);
					ImGui::Unindent(160);
				}
			ImGui::Unindent(160);
		}


		ImGui::NewLine();
		ImGui::Text("Instruction Count: %d", shaderDesc.InstructionCount);
		ImGui::Separator();
		ImGui::Indent(30);
			ImGui::Text("Non-categorized texture instructions: %d", shaderDesc.TextureNormalInstructions);
			ImGui::Text("Texture load instructions: %d", shaderDesc.TextureLoadInstructions);
			ImGui::Text("Texture comparison instructions: %d", shaderDesc.TextureCompInstructions);
			ImGui::Text("Texture bias instructions: %d", shaderDesc.TextureBiasInstructions);
			ImGui::Text("Texture gradient instructions: %d", shaderDesc.TextureGradientInstructions);
			ImGui::Text("Floating point arithmetic instructions: %d", shaderDesc.FloatInstructionCount);
			ImGui::Text("Signed integer arithmetic instructions: %d", shaderDesc.IntInstructionCount);
			ImGui::Text("Unsigned integer arithmetic instructions: %d", shaderDesc.UintInstructionCount);
			ImGui::Text("Static flow control instructions: %d", shaderDesc.StaticFlowControlCount);
			ImGui::Text("Dynamic flow control instructions: %d", shaderDesc.DynamicFlowControlCount);
			ImGui::Text("Macro instructions: %d", shaderDesc.MacroInstructionCount);
			ImGui::Text("Array instructions: %d", shaderDesc.ArrayInstructionCount);
			ImGui::Text("Cut instructions: %d", shaderDesc.CutInstructionCount);
			ImGui::Text("Emit instructions: %d", shaderDesc.EmitInstructionCount);
			ImGui::Text("Bitwise instructions: %d", shader->GetBitwiseInstructionCount());
			ImGui::Text("Conversion instructions: %d", shader->GetConversionInstructionCount());
			ImGui::Text("Movc instructions: %d", shader->GetMovcInstructionCount());
			ImGui::Text("Mov instructions: %d", shader->GetMovInstructionCount());
		ImGui::Unindent(30);

		ImGui::PopFont();
	}
}