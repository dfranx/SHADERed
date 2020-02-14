#include "PixelInspectUI.h"
#include "../Objects/DebugInformation.h"
#include "../Objects/Settings.h"
#include "../Objects/ShaderTranscompiler.h"
#include "CodeEditorUI.h"
#include "UIHelper.h"
#include "Icons.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#define ICON_BUTTON_WIDTH 25 * Settings::Instance().DPIScale
#define BUTTON_SIZE 20 * Settings::Instance().DPIScale

namespace ed
{
	void copyFloatData(eng::Model::Mesh::Vertex& out, GLfloat* bufData)
	{
		out.Position = glm::vec3(bufData[0], bufData[1], bufData[2]);
		out.Normal = glm::vec3(bufData[3], bufData[4], bufData[5]);
		out.TexCoords = glm::vec2(bufData[6], bufData[7]);
		out.Tangent = glm::vec3(bufData[8], bufData[9], bufData[10]);
		out.Binormal = glm::vec3(bufData[11], bufData[12], bufData[13]);
		out.Color = glm::vec4(bufData[14], bufData[15], bufData[16], bufData[17]);
	}
	void PixelInspectUI::OnEvent(const SDL_Event& e)
	{
	}
	void PixelInspectUI::Update(float delta)
	{
		ImVec2 containerSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y));
		std::vector<PixelInformation>& pixels = m_data->Debugger.GetPixelList();

		ImGui::BeginChild("##pixel_scroll_container", containerSize);

		int pxId = 0;

		for (auto& pixel : pixels) {
			ImGui::Text("%s(%s) - %s", pixel.Owner->Name, pixel.RenderTexture.c_str(), pixel.Object->Name);
			ImGui::PushItemWidth(-1);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::ColorEdit4("##pixel_edit", const_cast<float*>(glm::value_ptr(pixel.Color)));
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();

			if (!pixel.Fetched) {
				if (ImGui::Button(("Fetch##pixel_fetch_" + std::to_string(pxId)).c_str(), ImVec2(-1, 0))) {
					int vertID = m_data->Renderer.DebugVertexPick(pixel.Owner, pixel.Object, pixel.Coordinate.x, pixel.Coordinate.y);

					pixel.VertexID = vertID;

					// getting the vertices
					// TODO: lines, points, etc...
					int vertCount = 3;
					if (pixel.Object->Type == PipelineItem::ItemType::Geometry) {
						pipe::GeometryItem::GeometryType geoType = ((pipe::GeometryItem*)pixel.Object->Data)->Type;
						GLuint vbo = ((pipe::GeometryItem*)pixel.Object->Data)->VBO;

						// TODO: v1.3.*
						glBindBuffer(GL_ARRAY_BUFFER, vbo);
						if (geoType == pipe::GeometryItem::GeometryType::ScreenQuadNDC) {
							GLfloat bufData[4 * 4] = { 0.0f };
							glGetBufferSubData(GL_ARRAY_BUFFER, 0, 4 * 4 * sizeof(float), &bufData[0]);

							// TODO: change this *PLACEHOLDER*
							pixel.Vertex[0].Position = glm::vec3(bufData[0], bufData[1], 0.0f);
							pixel.Vertex[1].Position = glm::vec3(bufData[4], bufData[5], 0.0f);
							pixel.Vertex[2].Position = glm::vec3(0,0,0);
							pixel.VertexCount = vertCount;
						} else {
							GLfloat bufData[3 * 18] = { 0.0f };
							int vertStart = ((int)(vertID / vertCount)) * vertCount;
							glGetBufferSubData(GL_ARRAY_BUFFER, vertStart * 18 * sizeof(float), vertCount * 18 * sizeof(float), &bufData[0]);

							copyFloatData(pixel.Vertex[0], &bufData[0]);
							copyFloatData(pixel.Vertex[1], &bufData[18]);
							copyFloatData(pixel.Vertex[2], &bufData[36]);

							pixel.VertexCount = vertCount;
						}
						glBindBuffer(GL_ARRAY_BUFFER, 0);
					}
					else {
						int vertStart = ((int)(vertID / vertCount)) * vertCount;

						// TODO: mesh id??
						pipe::Model* mdl = ((pipe::Model*)pixel.Object->Data);
						pixel.Vertex[0] = mdl->Data->Meshes[0].Vertices[vertStart+0];
						pixel.Vertex[1] = mdl->Data->Meshes[0].Vertices[vertStart+1];
						pixel.Vertex[2] = mdl->Data->Meshes[0].Vertices[vertStart+2];
					}

					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

					// getting the debugger's vs output
					ed::ShaderLanguage lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->VSPath);
					std::string vsSrc = m_data->Parser.LoadProjectFile(pass->VSPath);
					bool vsCompiled = m_data->Debugger.SetSource(lang, sd::ShaderType::Vertex, lang == ed::ShaderLanguage::GLSL ? "main" : pass->VSEntry, vsSrc);
					if (vsCompiled) {
						for (int i = 0; i < vertCount; i++) {
							m_data->Debugger.InitEngine(pixel, i);
							m_data->Debugger.Fetch(i); // run the shader
						}
					}

					// TODO: pixel shader
					lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->PSPath);
					std::string psSrc = m_data->Parser.LoadProjectFile(pass->PSPath);
					bool psCompiled = m_data->Debugger.SetSource(lang, sd::ShaderType::Pixel, lang == ed::ShaderLanguage::GLSL ? "main" : pass->PSEntry, psSrc);
					if (psCompiled) {
						m_data->Debugger.InitEngine(pixel);
						m_data->Debugger.Fetch();
					}

					pixel.Discarded = m_data->Debugger.Engine.IsDiscarded();
					pixel.Fetched = vsCompiled && psCompiled;
				}
			}

			if (pixel.Fetched) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

				if (ImGui::Button(((UI_ICON_PLAY "##debug_pixel_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))) {
					m_data->Debugger.IsDebugging = true;

					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
					codeUI->OpenPS(pixel.Owner);

					ed::ShaderLanguage lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->PSPath);
					std::string psSrc = m_data->Parser.LoadProjectFile(pass->PSPath);
					bool psCompiled = m_data->Debugger.SetSource(lang, sd::ShaderType::Pixel, lang == ed::ShaderLanguage::GLSL ? "main" : pass->PSEntry, psSrc);
					if (psCompiled) {
						m_data->Debugger.InitEngine(pixel);

						TextEditor* editor = codeUI->GetPS(pixel.Owner);
						sd::ShaderDebugger* dbgr = &m_data->Debugger.Engine;
						const auto& bkpts = editor->GetBreakpoints();

						// skip initialization
						dbgr->Step();

						editor->SetCurrentLineIndicator(dbgr->GetCurrentLine());

						// copy breakpoints
						dbgr->ClearBreakpoints();
						for (const auto& bkpt : bkpts) {
							if (!bkpt.mEnabled) continue;

							if (bkpt.mCondition.empty()) dbgr->AddBreakpoint(bkpt.mLine);
							else dbgr->AddConditionalBreakpoint(bkpt.mLine, bkpt.mCondition);
						}

						// editor functions
						editor->OnBreakpointRemove = [&](TextEditor* ed, int line) {
							m_data->Debugger.Engine.ClearBreakpoint(line);
						};
						editor->OnBreakpointUpdate = [&](TextEditor* ed, int line, const std::string& cond, bool enabled) {
							if (!enabled) return;

							if (cond.empty()) m_data->Debugger.Engine.AddBreakpoint(line);
							else m_data->Debugger.Engine.AddConditionalBreakpoint(line, cond);
						};
						editor->OnDebuggerAction = [&](TextEditor* ed, TextEditor::DebugAction act) {
							sd::ShaderDebugger* mDbgr = &m_data->Debugger.Engine;
							bool state = false;
							switch (act) {
							case TextEditor::DebugAction::Continue:
								state = mDbgr->Continue();
								break;
							case TextEditor::DebugAction::Step:
								state = mDbgr->StepOver();
								break;
							case TextEditor::DebugAction::StepIn:
								state = mDbgr->Step();
								break;
							case TextEditor::DebugAction::StepOut:
								state = mDbgr->StepOut();
								break;
							}

							if (act == TextEditor::DebugAction::Stop || !state) {
								m_data->Debugger.IsDebugging = false;
								ed->SetCurrentLineIndicator(-1);
							}
							else
								ed->SetCurrentLineIndicator(mDbgr->GetCurrentLine());
						};
						editor->OnDebuggerJump = [&](TextEditor* ed, int line) {
							m_data->Debugger.Engine.Jump(line);
							ed->SetCurrentLineIndicator(m_data->Debugger.Engine.GetCurrentLine());
						};
						editor->HasIdentifierHover = [&](TextEditor* ed, const std::string& id) -> bool {
							sd::ShaderDebugger* mDbgr = &m_data->Debugger.Engine;
							bv_variable* var = mDbgr->GetLocalValue(id);
							if (var == nullptr)
								var = mDbgr->GetGlobalValue(id);

							return var != nullptr;
						};
						editor->OnIdentifierHover = [&](TextEditor* ed, const std::string& id) {
							sd::ShaderDebugger* mDbgr = &m_data->Debugger.Engine;
							bv_variable* var = mDbgr->GetLocalValue(id);
							if (var == nullptr)
								var = mDbgr->GetGlobalValue(id);
							if (var != nullptr) {
								// TODO: add variable type + seperator
								std::string value = UIHelper::GetVariableValue(*var);
								ImGui::Text(value.c_str());
							}
						};
					}
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
				ImGui::Button(((UI_ICON_PLAY "##debug_vertex0_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)); ImGui::SameLine();
				ImGui::Text("Vertex[0] = (%.2f, %.2f, %.2f)", pixel.Vertex[0].Position.x, pixel.Vertex[0].Position.y, pixel.Vertex[0].Position.z);

				ImGui::Button(((UI_ICON_PLAY "##debug_vertex1_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)); ImGui::SameLine();
				ImGui::Text("Vertex[1] = (%.2f, %.2f, %.2f)", pixel.Vertex[1].Position.x, pixel.Vertex[1].Position.y, pixel.Vertex[1].Position.z);

				ImGui::Button(((UI_ICON_PLAY "##debug_vertex2_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)); ImGui::SameLine();
				ImGui::Text("Vertex[2] = (%.2f, %.2f, %.2f)", pixel.Vertex[2].Position.x, pixel.Vertex[2].Position.y, pixel.Vertex[2].Position.z);

				ImGui::PopStyleColor();
			}

			ImGui::Separator();
			ImGui::NewLine();

			pxId++;
		}
		
		ImGui::EndChild();
	}
}