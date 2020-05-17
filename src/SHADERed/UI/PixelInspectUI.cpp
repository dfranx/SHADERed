#include <SHADERed/Objects/DebugInformation.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/Debug/FunctionStackUI.h>
#include <SHADERed/UI/Debug/WatchUI.h>
#include <SHADERed/UI/Icons.h>
#include <SHADERed/UI/PixelInspectUI.h>
#include <SHADERed/UI/UIHelper.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#define ICON_BUTTON_WIDTH Settings::Instance().CalculateSize(25)
#define BUTTON_SIZE Settings::Instance().CalculateSize(20)

namespace ed {
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
			/* [PASS NAME, RT NAME, OBJECT NAME, COORDINATE] */
			ImGui::Text("%s(%s) - %s at (%d,%d)", pixel.Pass->Name, pixel.RenderTexture.empty() ? "Window" : pixel.RenderTexture.c_str(), pixel.Object->Name, pixel.Coordinate.x, pixel.Coordinate.y);
			
			/* [PIXEL COLOR] */
			ImGui::PushItemWidth(-1);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::ColorEdit4("##pixel_edit", const_cast<float*>(glm::value_ptr(pixel.Color)));
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();

			m_ui->Get(ViewID::PixelInspect);

			if (!pixel.Fetched) {
				if (ImGui::Button(("Fetch##pixel_fetch_" + std::to_string(pxId)).c_str(), ImVec2(-1, 0))
					&& m_data->Messages.CanRenderPreview()) {
					m_data->FetchPixel(pixel);
				}
			} else {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

				TextEditor* editor = nullptr;
				bool requestCompile = false;
				int initIndex = 0;


				/* [PIXEL] */
				if (ImGui::Button(((UI_ICON_PLAY "##debug_pixel_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))
					&& m_data->Messages.CanRenderPreview()) {
					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Pass->Data);

					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
					codeUI->StopDebugging();
					codeUI->Open(pixel.Pass, ShaderStage::Pixel);
					editor = codeUI->Get(pixel.Pass, ShaderStage::Pixel);

					m_data->Debugger.PreparePixelShader(pixel.Pass, pixel.Object);
					m_data->Debugger.SetPixelShaderInput(pixel);
					requestCompile = true;
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


				/* [VERTEX] */
				for (int i = 0; i < pixel.VertexCount; i++) {
					if (ImGui::Button(((UI_ICON_PLAY "##debug_vertex" + std::to_string(i) + "_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))
						&& m_data->Messages.CanRenderPreview()) {
						pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Pass->Data);

						CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
						codeUI->StopDebugging();
						codeUI->Open(pixel.Pass, ShaderStage::Vertex);
						editor = codeUI->Get(pixel.Pass, ShaderStage::Vertex);

						m_data->Debugger.PrepareVertexShader(pixel.Pass, pixel.Object);
						m_data->Debugger.SetVertexShaderInput(pass, pixel.Vertex[i], pixel.VertexID + i, pixel.InstanceID, (BufferObject*)pixel.InstanceBuffer);
						
						initIndex = i;
						requestCompile = true;
					}
					ImGui::SameLine();
					ImGui::Text("Vertex[%d] = (%.2f, %.2f, %.2f)", i, pixel.Vertex[i].Position.x, pixel.Vertex[i].Position.y, pixel.Vertex[i].Position.z);
				}

				/* [TODO:GEOMETRY SHADER]*/


				/* ACTUAL ACTION HERE */
				if (requestCompile) {
					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));

					m_data->Debugger.SetCurrentFile(editor->GetPath());
					m_data->Debugger.SetDebugging(true);
					m_data->Debugger.PrepareDebugger();
				
					// skip initialization
					editor->SetCurrentLineIndicator(m_data->Debugger.GetCurrentLine());

					// reset function stack
					((DebugFunctionStackUI*)m_ui->Get(ViewID::DebugFunctionStack))->Refresh();

					// update watch values
					const std::vector<char*>& watchList = m_data->Debugger.GetWatchList();
					for (size_t i = 0; i < watchList.size(); i++)
						m_data->Debugger.UpdateWatchValue(i);

					// editor functions
					editor->OnDebuggerAction = [&](TextEditor* ed, TextEditor::DebugAction act) {
						if (!m_data->Debugger.IsDebugging())
							return;

						bool state = m_data->Debugger.IsVMRunning();
						switch (act) {
						case TextEditor::DebugAction::Continue:
							m_data->Debugger.Continue();
							break;
						case TextEditor::DebugAction::Step:
							m_data->Debugger.Step();
							break;
						case TextEditor::DebugAction::StepInto:
							m_data->Debugger.StepInto();
							break;
						case TextEditor::DebugAction::StepOut:
							m_data->Debugger.StepOut();
							break;
						}

						if (m_data->Debugger.GetVM()->discarded)
							state = false;

						if (act == TextEditor::DebugAction::Stop || !state) {
							m_data->Debugger.SetDebugging(false);
							ed->SetCurrentLineIndicator(-1);
						} else {
							((DebugWatchUI*)m_ui->Get(ViewID::DebugWatch))->Refresh();
							((DebugFunctionStackUI*)m_ui->Get(ViewID::DebugFunctionStack))->Refresh();
													
							int curLine = m_data->Debugger.GetCurrentLine();
							if (!m_data->Debugger.IsVMRunning())
								curLine++;
							ed->SetCurrentLineIndicator(curLine);
						}
					};
					editor->OnDebuggerJump = [&](TextEditor* ed, int line) {
						if (!m_data->Debugger.IsDebugging())
							return;

						while (m_data->Debugger.IsVMRunning() && m_data->Debugger.GetCurrentLine() != line)
							spvm_state_step_into(m_data->Debugger.GetVM());

						ed->SetCurrentLineIndicator(m_data->Debugger.GetCurrentLine());
						((DebugWatchUI*)m_ui->Get(ViewID::DebugWatch))->Refresh();
					};
					editor->HasIdentifierHover = [&](TextEditor* ed, const std::string& id) -> bool {
						if (!m_data->Debugger.IsDebugging() || !Settings::Instance().Debug.ShowValuesOnHover)
							return false;

						size_t vcount = 0;
						spvm_member_t res = m_data->Debugger.GetVariable(id, vcount);
						
						return res != nullptr;
					};
					editor->OnIdentifierHover = [&](TextEditor* ed, const std::string& id) {
						size_t vcount = 0;
						spvm_result_t resType = nullptr;
						spvm_member_t res = m_data->Debugger.GetVariable(id, vcount, resType);
						
						if (res != nullptr && resType != nullptr) {

							bool isTex = false,
								 isCube = false;

							// Type:
							if (resType->value_type == spvm_value_type_int && resType->value_sign == 0)
								ImGui::Text("uint");
							else if (resType->value_type == spvm_value_type_int)
								ImGui::Text("int");
							else if (resType->value_type == spvm_value_type_bool)
								ImGui::Text("bool");
							else if (resType->value_type == spvm_value_type_float && resType->value_bitcount > 32)
								ImGui::Text("double");
							else if (resType->value_type == spvm_value_type_float)
								ImGui::Text("float");
							else if (resType->value_type == spvm_value_type_matrix)
								ImGui::Text("matrix");
							else if (resType->value_type == spvm_value_type_vector)
								ImGui::Text("vector");
							else if (resType->value_type == spvm_value_type_array)
								ImGui::Text("array");
							else if (resType->value_type == spvm_value_type_runtime_array)
								ImGui::Text("runtime array");
							else if (resType->value_type == spvm_value_type_struct && resType->name)
								ImGui::Text(resType->name);
							else if (resType->value_type == spvm_value_type_sampled_image || resType->value_type == spvm_value_type_image) {
								isTex = true;
								ImGui::Text("texture");

								if (resType && resType->image_info->dim == SpvDimCube)
									isCube = true;
							}

							ImGui::Separator();

							// Value:
							if (isTex && res->image_data != nullptr) {
								spvm_image_t tex = res->image_data;
								
								if (isCube) {
									m_cubePrev.Draw((GLuint)tex->user_data);
									ImGui::Image((ImTextureID)m_cubePrev.GetTexture(), ImVec2(128.0f, 128.0f * (375.0f / 512.0f)), ImVec2(0, 1), ImVec2(1, 0));
								} else
									ImGui::Image((ImTextureID)tex->user_data, ImVec2(128.0f, 128.0f * (tex->height / (float)tex->width)), ImVec2(0, 1), ImVec2(1, 0));
							} else {
								std::stringstream ss;
								m_data->Debugger.GetVariableValueAsString(ss, resType, res, vcount, "");
								ImGui::Text(ss.str().c_str());
							}
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
	}
}