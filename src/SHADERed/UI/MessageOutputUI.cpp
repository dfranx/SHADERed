#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <SHADERed/Options.h>
#include <SHADERed/UI/MessageOutputUI.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <imgui/imgui.h>

namespace ed {
	void MessageOutputUI::OnEvent(const SDL_Event& e)
	{
	}
	void MessageOutputUI::Update(float delta)
	{
		const std::vector<MessageStack::Message>& msgs = m_data->Messages.GetMessages();

		if (ImGui::BeginTable("##msg_table", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollFreezeTopRow | ImGuiTableFlags_ScrollY)) {
			ImGui::TableSetupColumn("Shader Pass", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableAutoHeaders();

			int rowIndex = 0;
			const ed::CustomColors& clrs = ThemeContainer::Instance().GetCustomStyle(Settings::Instance().Theme);

			for (int i = 0; i < msgs.size(); i++) {
				ImGui::TableNextRow();

				const MessageStack::Message* m = &msgs[i];

				ImVec4 color = clrs.InfoMessage;
				if (m->MType == MessageStack::Type::Error)
					color = clrs.ErrorMessage;
				else if (m->MType == MessageStack::Type::Warning)
					color = clrs.WarningMessage;

				ImGui::TableSetColumnIndex(0);
				ImGui::PushID(i);
				if (ImGui::Selectable(m->Group.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
						ed::PipelineItem* pass = m_data->Pipeline.Get(m->Group.c_str());

						if (pass != nullptr) {
							CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
							if (pass->Type == PipelineItem::ItemType::ShaderPass && m->Shader != ShaderStage::Count)
								codeUI->Open(pass, m->Shader);
							else if (pass->Type == PipelineItem::ItemType::ComputePass && m->Shader != ShaderStage::Count)
								codeUI->Open(pass, m->Shader);
							else if (pass->Type == PipelineItem::ItemType::AudioPass && m->Shader != ShaderStage::Count)
								codeUI->Open(pass, m->Shader);
							else if (pass->Type == PipelineItem::ItemType::PluginItem) {
								pipe::PluginItemData* plData = ((pipe::PluginItemData*)pass->Data);
								plData->Owner->PipelineItem_OpenInEditor(plData->Type, plData->PluginData);
							}

							TextEditor* editor = codeUI->Get(pass, m->Shader);
							if (editor != nullptr && m->Line != -1)
								editor->SetCursorPosition(TextEditor::Coordinates(std::max<int>(0, m->Line - 1), 0));
						}
					}
				}
				// context menu -- "Copy" button
				if (ImGui::BeginPopupContextItem("##context_main_msgs")) {
					if (ImGui::Selectable("Copy"))
						ImGui::SetClipboardText(m->Text.c_str());

					ImGui::EndPopup();
				}
				ImGui::PopID();

				ImGui::TableSetColumnIndex(1);
				if (m->Shader != ShaderStage::Count) {
					// TODO: array? duh
					std::string stageAbbr = "VS";
					if (m->Shader == ShaderStage::Pixel) stageAbbr = "PS";
					else if (m->Shader == ShaderStage::Geometry) stageAbbr = "GS";
					else if (m->Shader == ShaderStage::Compute) stageAbbr = "CS";
					else if (m->Shader == ShaderStage::Audio) stageAbbr = "AS";
					else if (m->Shader == ShaderStage::TessellationControl) stageAbbr = "TCS";
					else if (m->Shader == ShaderStage::TessellationEvaluation) stageAbbr = "TES";
					ImGui::TextUnformatted(stageAbbr.c_str());
				}

				ImGui::TableSetColumnIndex(2);
				if (m->Line != -1)
					ImGui::Text(std::to_string(m->Line).c_str());

				ImGui::TableSetColumnIndex(3);
				ImGui::TextColored(color, m->Text.c_str());
			}

			ImGui::EndTable();
		}
	}
}