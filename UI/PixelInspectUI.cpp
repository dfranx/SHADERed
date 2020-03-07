#include "PixelInspectUI.h"
#include "../Objects/DebugInformation.h"
#include "../Objects/Settings.h"
#include "../Objects/ShaderTranscompiler.h"
#include "Debug/WatchUI.h"
#include "Debug/FunctionStackUI.h"
#include "CodeEditorUI.h"
#include "UIHelper.h"
#include "Icons.h"

#include <ShaderDebugger/Utils.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#define ICON_BUTTON_WIDTH Settings::Instance().CalculateSize(25)
#define BUTTON_SIZE Settings::Instance().CalculateSize(20)

namespace ed
{
	void PixelInspectUI::OnEvent(const SDL_Event& e)
	{
	}
	void PixelInspectUI::Update(float delta)
	{
		std::vector<PixelInformation>& pixels = m_data->Debugger.GetPixelList();

		if (ImGui::Button("Clear##pixel_clear", ImVec2(-1, 0)))
			pixels.clear();

		ImGui::NewLine();

		ImGui::BeginChild("##pixel_scroll_container", ImVec2(-1, -1));

		int pxId = 0;

		for (auto& pixel : pixels) {
			ImGui::Text("%s(%s) - %s", pixel.Owner->Name, pixel.RenderTexture.c_str(), pixel.Object->Name);
			ImGui::PushItemWidth(-1);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::ColorEdit4("##pixel_edit", const_cast<float*>(glm::value_ptr(pixel.Color)));
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();

			if (!pixel.Fetched) {
				if (ImGui::Button(("Fetch##pixel_fetch_" + std::to_string(pxId)).c_str(), ImVec2(-1, 0))
					&& m_data->Messages.CanRenderPreview())
				{
					bool success = m_data->FetchPixel(pixel);

					if (!success) {
						m_errorPopup = true;
						m_errorMessage = m_data->Debugger.Engine.GetLastError();
					}
				}
			}

			if (pixel.Fetched) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

				TextEditor* editor = nullptr;
				bool requestCompile = false;
				int initIndex = 0;

				if (ImGui::Button(((UI_ICON_PLAY "##debug_pixel_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))
					&& m_data->Messages.CanRenderPreview())
				{
					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
					codeUI->StopDebugging();
					codeUI->Open(pixel.Owner, ShaderStage::Pixel);
					editor = codeUI->Get(pixel.Owner, ShaderStage::Pixel);

					ed::ShaderLanguage lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->PSPath);
					std::string psSrc = m_data->Parser.LoadProjectFile(pass->PSPath);
					requestCompile = m_data->Debugger.SetSource(lang, sd::ShaderType::Pixel, lang == ed::ShaderLanguage::GLSL ? "main" : pass->PSEntry, psSrc);
				}
				ImGui::SameLine();
				if (pixel.Discarded)
					ImGui::Text("discarded");
				else {
					ImGui::PushItemWidth(-1);
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::ColorEdit4("##dbg_pixel_edit", const_cast<float*>(glm::value_ptr(pixel.DebuggerColor)));
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
				}

				// TODO:
				if (ImGui::Button(((UI_ICON_PLAY "##debug_vertex0_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))
					&& m_data->Messages.CanRenderPreview())
				{
					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
					codeUI->StopDebugging();
					codeUI->Open(pixel.Owner, ShaderStage::Vertex);
					editor = codeUI->Get(pixel.Owner, ShaderStage::Vertex);

					ed::ShaderLanguage lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->VSPath);
					std::string vsSrc = m_data->Parser.LoadProjectFile(pass->VSPath);
					requestCompile = m_data->Debugger.SetSource(lang, sd::ShaderType::Vertex, lang == ed::ShaderLanguage::GLSL ? "main" : pass->VSEntry, vsSrc);
					initIndex = 0;
				}
				ImGui::SameLine(); 
				ImGui::Text("Vertex[0] = (%.2f, %.2f, %.2f)", pixel.Vertex[0].Position.x, pixel.Vertex[0].Position.y, pixel.Vertex[0].Position.z);

				if (ImGui::Button(((UI_ICON_PLAY "##debug_vertex1_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))
					&& m_data->Messages.CanRenderPreview())
				{
					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
					codeUI->StopDebugging();
					codeUI->Open(pixel.Owner, ShaderStage::Vertex);
					editor = codeUI->Get(pixel.Owner, ShaderStage::Vertex);

					ed::ShaderLanguage lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->VSPath);
					std::string vsSrc = m_data->Parser.LoadProjectFile(pass->VSPath);
					requestCompile = m_data->Debugger.SetSource(lang, sd::ShaderType::Vertex, lang == ed::ShaderLanguage::GLSL ? "main" : pass->VSEntry, vsSrc);
					initIndex = 1;
				}
				ImGui::SameLine();
				ImGui::Text("Vertex[1] = (%.2f, %.2f, %.2f)", pixel.Vertex[1].Position.x, pixel.Vertex[1].Position.y, pixel.Vertex[1].Position.z);


				if (ImGui::Button(((UI_ICON_PLAY "##debug_vertex2_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))
					&& m_data->Messages.CanRenderPreview())
				{
					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
					codeUI->StopDebugging();
					codeUI->Open(pixel.Owner, ShaderStage::Vertex);
					editor = codeUI->Get(pixel.Owner, ShaderStage::Vertex);

					ed::ShaderLanguage lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->VSPath);
					std::string vsSrc = m_data->Parser.LoadProjectFile(pass->VSPath);
					requestCompile = m_data->Debugger.SetSource(lang, sd::ShaderType::Vertex, lang == ed::ShaderLanguage::GLSL ? "main" : pass->VSEntry, vsSrc);
					initIndex = 2;
				}
				ImGui::SameLine();
				ImGui::Text("Vertex[2] = (%.2f, %.2f, %.2f)", pixel.Vertex[2].Position.x, pixel.Vertex[2].Position.y, pixel.Vertex[2].Position.z);


				if (requestCompile) {
					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));

					m_data->Debugger.SetDebugging(true);
					m_data->Debugger.InitEngine(pixel, initIndex);
					
					sd::ShaderDebugger* dbgr = &m_data->Debugger.Engine;
					const auto& bkpts = editor->GetBreakpoints();

					// skip initialization
					dbgr->Step();
					editor->SetCurrentLineIndicator(dbgr->GetCurrentLine());

					// reset function stack
					((DebugFunctionStackUI*)m_ui->Get(ViewID::DebugFunctionStack))->Refresh();

					// update watch values
					const std::vector<char*>& watchList = m_data->Debugger.GetWatchList();
					for (size_t i = 0; i < watchList.size(); i++)
						m_data->Debugger.UpdateWatchValue(i);

					// copy breakpoints
					dbgr->ClearBreakpoints();
					for (const auto& bkpt : bkpts) {
						if (!bkpt.mEnabled) continue;

						if (bkpt.mCondition.empty()) dbgr->AddBreakpoint(bkpt.mLine);
						else dbgr->AddConditionalBreakpoint(bkpt.mLine, bkpt.mCondition);
					}

					// editor functions
					editor->OnDebuggerAction = [&](TextEditor* ed, TextEditor::DebugAction act) {
						if (!m_data->Debugger.IsDebugging())
							return;

						sd::ShaderDebugger* mDbgr = &m_data->Debugger.Engine;
						bool state = false;
						switch (act) {
						case TextEditor::DebugAction::Continue:
							state = mDbgr->Continue();
							break;
						case TextEditor::DebugAction::Step:
							state = mDbgr->StepOver();
							break;
						case TextEditor::DebugAction::StepInto:
							state = mDbgr->Step();
							break;
						case TextEditor::DebugAction::StepOut:
							state = mDbgr->StepOut();
							break;
						}

						if (act == TextEditor::DebugAction::Stop || !state) {
							m_data->Debugger.SetDebugging(false);
							ed->SetCurrentLineIndicator(-1);
						}
						else {
							((DebugWatchUI*)m_ui->Get(ViewID::DebugWatch))->Refresh();
							((DebugFunctionStackUI*)m_ui->Get(ViewID::DebugFunctionStack))->Refresh();
							ed->SetCurrentLineIndicator(mDbgr->GetCurrentLine());
						}
					};
					editor->OnDebuggerJump = [&](TextEditor* ed, int line) {
						if (!m_data->Debugger.IsDebugging())
							return;

						m_data->Debugger.Engine.Jump(line);
						ed->SetCurrentLineIndicator(m_data->Debugger.Engine.GetCurrentLine());
						((DebugWatchUI*)m_ui->Get(ViewID::DebugWatch))->Refresh();
					};
					editor->HasIdentifierHover = [&](TextEditor* ed, const std::string& id) -> bool {
						if (!m_data->Debugger.IsDebugging())
							return false;

						sd::ShaderDebugger* mDbgr = &m_data->Debugger.Engine;
						bv_variable* var = mDbgr->GetLocalValue(id);
						if (var == nullptr)
							var = mDbgr->GetGlobalValue(id);

						return var != nullptr && var->type != bv_type_void && Settings::Instance().Debug.ShowValuesOnHover;
					};
					editor->OnIdentifierHover = [&](TextEditor* ed, const std::string& id) {
						sd::ShaderDebugger* mDbgr = &m_data->Debugger.Engine;
						bv_variable* var = mDbgr->GetLocalValue(id);
						if (var == nullptr)
							var = mDbgr->GetGlobalValue(id);
						if (var != nullptr && var->type != bv_type_void) {
							char* objTypename = nullptr;

							// Type:
							if (var->type == bv_type_int)
								ImGui::Text("int");
							else if (var->type == bv_type_short)
								ImGui::Text("short");
							else if (var->type == bv_type_char || var->type == bv_type_uchar)
								ImGui::Text("bool");
							else if (var->type == bv_type_uint)
								ImGui::Text("uint");
							else if (var->type == bv_type_ushort)
								ImGui::Text("ushort");
							else if (var->type == bv_type_float)
								ImGui::Text("float");
							else if (var->type == bv_type_object) {
								objTypename = bv_variable_get_object(*var)->type->name;
								ImGui::Text(objTypename);
							}

							ImGui::Separator();

							// Value:
							bool isTex = false, isCube = false;
							if (var->type == bv_type_object) {
								isTex = sd::IsBasicTexture(objTypename);
								isCube = sd::IsCubemap(objTypename);
							}

							if (isTex) {
								sd::Texture* tex = (sd::Texture*)bv_variable_get_object(*var)->user_data;
								ImGui::Image((ImTextureID)tex->UserData, ImVec2(128.0f, 128.0f * (tex->Height / (float)tex->Width)), ImVec2(0, 1), ImVec2(1, 0));
							}
							else if (isCube) {
								sd::TextureCube* tex = (sd::TextureCube*)bv_variable_get_object(*var)->user_data;
								m_cubePrev.Draw(tex->UserData);
								ImGui::Image((ImTextureID)m_cubePrev.GetTexture(), ImVec2(128.0f, 128.0f * (375.0f / 512.0f)), ImVec2(0, 1), ImVec2(1, 0));
							}
							else
								ImGui::Text(m_data->Debugger.VariableValueToString(*var).c_str());
						}
					};
				}

				ImGui::PopStyleColor();
			}

			ImGui::Separator();
			ImGui::NewLine();

			pxId++;
		}
		
		ImGui::EndChild();

		if (m_errorPopup) {
			ImGui::OpenPopup("Error message##sdbg_error_msg");
			m_errorPopup = false;
		}

		// Create Item popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(125)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Error message##sdbg_error_msg", 0, ImGuiWindowFlags_NoResize)) {
			ImGui::Text("ERROR: ");
			ImGui::Text(m_errorMessage.c_str());
			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}
}