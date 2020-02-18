#include "PixelInspectUI.h"
#include "../Objects/DebugInformation.h"
#include "../Objects/Settings.h"
#include "../Objects/ShaderTranscompiler.h"
#include "Debug/WatchUI.h"
#include "CodeEditorUI.h"
#include "UIHelper.h"
#include "Icons.h"

#include <ShaderDebugger/Utils.h>
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
					int vertID = m_data->Renderer.DebugVertexPick(pixel.Owner, pixel.Object, pixel.RelativeCoordinate);
					bool isInstanced = false;

					pixel.VertexID = vertID;

					// getting the vertices
					// TODO: lines, points, etc...
					int vertCount = 3;
					if (pixel.Object->Type == PipelineItem::ItemType::Geometry) {
						pipe::GeometryItem::GeometryType geoType = ((pipe::GeometryItem*)pixel.Object->Data)->Type;
						GLuint vbo = ((pipe::GeometryItem*)pixel.Object->Data)->VBO;

						isInstanced = ((pipe::GeometryItem*)pixel.Object->Data)->Instanced;

						// TODO: don't bother GPU so much, v1.3.*
						glBindBuffer(GL_ARRAY_BUFFER, vbo);
						if (geoType == pipe::GeometryItem::GeometryType::ScreenQuadNDC) {
							GLfloat bufData[6 * 4] = { 0.0f };
							glGetBufferSubData(GL_ARRAY_BUFFER, 0, 6 * 4 * sizeof(float), &bufData[0]);


							int bufferLoc = (pixel.VertexID / vertCount) * vertCount * 4;

							// TODO: change this *PLACEHOLDER*
							pixel.Vertex[0].Position = glm::vec3(bufData[bufferLoc + 0], bufData[bufferLoc + 1], 0.0f);
							pixel.Vertex[1].Position = glm::vec3(bufData[bufferLoc + 4], bufData[bufferLoc + 5], 0.0f);
							pixel.Vertex[2].Position = glm::vec3(bufData[bufferLoc + 8], bufData[bufferLoc + 9], 0.0f);
							pixel.Vertex[0].TexCoords = glm::vec2(bufData[bufferLoc + 2], bufData[bufferLoc + 3]);
							pixel.Vertex[1].TexCoords = glm::vec2(bufData[bufferLoc + 6], bufData[bufferLoc + 7]);
							pixel.Vertex[2].TexCoords = glm::vec2(bufData[bufferLoc + 10], bufData[bufferLoc + 11]);
						}
						else {
							GLfloat bufData[3 * 18] = { 0.0f };
							int vertStart = ((int)(vertID / vertCount)) * vertCount;
							glGetBufferSubData(GL_ARRAY_BUFFER, vertStart * 18 * sizeof(float), vertCount * 18 * sizeof(float), &bufData[0]);

							copyFloatData(pixel.Vertex[0], &bufData[0]);
							copyFloatData(pixel.Vertex[1], &bufData[18]);
							copyFloatData(pixel.Vertex[2], &bufData[36]);
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

						isInstanced = mdl->Instanced;
					}
					pixel.VertexCount = vertCount;

					// get the instance id if this object uses instancing
					if (isInstanced)
						pixel.InstanceID = m_data->Renderer.DebugInstancePick(pixel.Owner, pixel.Object, pixel.RelativeCoordinate);

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

					// getting the debugger's ps output
					bool psCompiled = false;
					if (vsCompiled) {
						lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->PSPath);
						std::string psSrc = m_data->Parser.LoadProjectFile(pass->PSPath);
						psCompiled = m_data->Debugger.SetSource(lang, sd::ShaderType::Pixel, lang == ed::ShaderLanguage::GLSL ? "main" : pass->PSEntry, psSrc);
						if (psCompiled) {
							m_data->Debugger.InitEngine(pixel);
							m_data->Debugger.Fetch();
						}
					}

					pixel.Discarded = m_data->Debugger.Engine.IsDiscarded();
					pixel.Fetched = vsCompiled && psCompiled;
				}
			}

			if (pixel.Fetched) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

				TextEditor* editor = nullptr;
				bool requestCompile = false;
				int initIndex = 0;

				if (ImGui::Button(((UI_ICON_PLAY "##debug_pixel_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))) {
					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
					codeUI->StopDebugging();
					codeUI->OpenPS(pixel.Owner);
					editor = codeUI->GetPS(pixel.Owner);

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
				if (ImGui::Button(((UI_ICON_PLAY "##debug_vertex0_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))) {
					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
					codeUI->StopDebugging();
					codeUI->OpenVS(pixel.Owner);
					editor = codeUI->GetVS(pixel.Owner);

					ed::ShaderLanguage lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->VSPath);
					std::string vsSrc = m_data->Parser.LoadProjectFile(pass->VSPath);
					requestCompile = m_data->Debugger.SetSource(lang, sd::ShaderType::Vertex, lang == ed::ShaderLanguage::GLSL ? "main" : pass->VSEntry, vsSrc);
					initIndex = 0;
				}
				ImGui::SameLine(); 
				ImGui::Text("Vertex[0] = (%.2f, %.2f, %.2f)", pixel.Vertex[0].Position.x, pixel.Vertex[0].Position.y, pixel.Vertex[0].Position.z);

				if (ImGui::Button(((UI_ICON_PLAY "##debug_vertex1_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))) {
					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
					codeUI->StopDebugging();
					codeUI->OpenVS(pixel.Owner);
					editor = codeUI->GetVS(pixel.Owner);

					ed::ShaderLanguage lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->VSPath);
					std::string vsSrc = m_data->Parser.LoadProjectFile(pass->VSPath);
					requestCompile = m_data->Debugger.SetSource(lang, sd::ShaderType::Vertex, lang == ed::ShaderLanguage::GLSL ? "main" : pass->VSEntry, vsSrc);
					initIndex = 1;
				}
				ImGui::SameLine();
				ImGui::Text("Vertex[1] = (%.2f, %.2f, %.2f)", pixel.Vertex[1].Position.x, pixel.Vertex[1].Position.y, pixel.Vertex[1].Position.z);


				if (ImGui::Button(((UI_ICON_PLAY "##debug_vertex2_") + std::to_string(pxId)).c_str(), ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))) {
					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
					codeUI->StopDebugging();
					codeUI->OpenVS(pixel.Owner);
					editor = codeUI->GetVS(pixel.Owner);

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
							m_data->Debugger.SetDebugging(false);
							ed->SetCurrentLineIndicator(-1);
						}
						else {
							((DebugWatchUI*)m_ui->Get(ViewID::DebugWatch))->Refresh();
							ed->SetCurrentLineIndicator(mDbgr->GetCurrentLine());
						}
					};
					editor->OnDebuggerJump = [&](TextEditor* ed, int line) {
						m_data->Debugger.Engine.Jump(line);
						ed->SetCurrentLineIndicator(m_data->Debugger.Engine.GetCurrentLine());
						((DebugWatchUI*)m_ui->Get(ViewID::DebugWatch))->Refresh();
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
							else if (var->type == bv_type_object)
								ImGui::Text(bv_variable_get_object(*var)->type->name);

							ImGui::Separator();

							// Value:
							bool isTex = false;
							if (var->type == bv_type_object)
								isTex = sd::IsBasicTexture(bv_variable_get_object(*var)->type->name);

							if (isTex) {
								sd::Texture* tex = (sd::Texture*)bv_variable_get_object(*var)->user_data;
								ImGui::Image((ImTextureID)tex->UserData, ImVec2(128.0f, 128.0f * (tex->Height / (float)tex->Width)), ImVec2(0, 1), ImVec2(1, 0));
							}
							else {
								std::string value = UIHelper::GetVariableValue(*var);
								ImGui::Text(value.c_str());
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