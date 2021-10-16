#include <SHADERed/Objects/DebugInformation.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/ArcBallCamera.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/Debug/AutoUI.h>
#include <SHADERed/UI/Debug/FunctionStackUI.h>
#include <SHADERed/UI/Debug/GeometryOutputUI.h>
#include <SHADERed/UI/Debug/TessellationControlOutputUI.h>
#include <SHADERed/UI/Debug/WatchUI.h>
#include <SHADERed/UI/Debug/ValuesUI.h>
#include <SHADERed/UI/Debug/VectorWatchUI.h>
#include <SHADERed/UI/Icons.h>
#include <SHADERed/UI/PixelInspectUI.h>
#include <SHADERed/UI/UIHelper.h>

#include <imgui/imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
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
		std::vector<DebuggerSuggestion>& suggestions = m_data->Debugger.GetSuggestionList();

		if (pixels.size() != m_pixelHeights.size())
			m_pixelHeights.resize(pixels.size());

		ImGui::BeginChild("##pixel_scroll_container", ImVec2(-1, -1));

		if (!m_data->Renderer.IsPaused())
			ImGui::TextWrapped("Pause the execution in order to select a pixel to inspect.");
		else if (pixels.size() == 0)
			ImGui::TextWrapped("Click on a pixel to inspect it.");


		ImVec4 childBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, childBg * ImVec4(0.9f, 0.9f, 0.9f, 0.9f));

		// pixel/vertex/geometry
		int pxId = 0;
		for (auto& pixel : pixels) {
			if (pixel.History)
				continue;

			ImGui::PushID(pxId);
			
			this->RenderPixelInfo(pixel, m_pixelHeights[pxId]);

			ImGui::PopID();
			pxId++;
		}

		if (suggestions.size()) {
			if (suggestions.size() != m_suggestionHeights.size())
				m_suggestionHeights.resize(suggestions.size());

			ImGui::Text("Suggestions:");
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

			// suggestions
			int sugId = 0;
			for (auto& suggestion : suggestions) {
				ImGui::PushID(sugId);

				if (ImGui::BeginChild("##suggestion_container", ImVec2(0, m_suggestionHeights[sugId]), true)) {
					float pxCursorStart = ImGui::GetCursorPosY();
					if (suggestion.Type == DebuggerSuggestion::SuggestionType::ComputeShader) {
						pipe::ComputePass* pass = (pipe::ComputePass*)suggestion.Item->Data;
						glm::ivec3 thread = suggestion.Thread;
						ed::ShaderLanguage lang = ed::ShaderCompiler::GetShaderLanguageFromExtension(pass->Path);

						if (suggestion.WorkgroupSize.x == 0) {
							SPIRVParser parser;
							parser.Parse(pass->SPV);

							suggestion.WorkgroupSize = glm::ivec3(parser.LocalSizeX, parser.LocalSizeY, parser.LocalSizeZ);
						}

						if (ImGui::Button(UI_ICON_PLAY "##debug_compute", ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))) {

							pipe::ComputePass* pass = ((pipe::ComputePass*)suggestion.Item->Data);
							// TODO: plugins?

							CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
							codeUI->StopDebugging();
							codeUI->Open(suggestion.Item, ShaderStage::Compute);
							TextEditor* editor = codeUI->Get(suggestion.Item, ShaderStage::Compute);
							PluginShaderEditor pluginEditor;
							if (editor == nullptr)
								pluginEditor = codeUI->GetPluginEditor(suggestion.Item, ShaderStage::Compute);

							m_data->Debugger.PrepareComputeShader(suggestion.Item, suggestion.Thread.x, suggestion.Thread.y, suggestion.Thread.z);
							this->StartDebugging(editor, pluginEditor, nullptr);
						}
						ImGui::SameLine();

						/* [PASS NAME, THREAD ID] */
						ImGui::Text("%s @ (%d,%d,%d)", suggestion.Item->Name, suggestion.Thread.x, suggestion.Thread.y, suggestion.Thread.z);

						/* THREAD INFO */
						int localInvocationIndex = (thread.z % suggestion.WorkgroupSize.z) * suggestion.WorkgroupSize.x * suggestion.WorkgroupSize.z + (thread.y % suggestion.WorkgroupSize.y) * suggestion.WorkgroupSize.x + (thread.x % suggestion.WorkgroupSize.x);

						if (lang == ed::ShaderLanguage::HLSL) {
							ImGui::Text("SV_GroupID -> uint3(%d, %d, %d)", thread.x / suggestion.WorkgroupSize.x, thread.y / suggestion.WorkgroupSize.y, thread.z / suggestion.WorkgroupSize.z);
							ImGui::Text("SV_GroupThreadID -> uint3(%d, %d, %d)", thread.x % suggestion.WorkgroupSize.x, thread.y % suggestion.WorkgroupSize.y, thread.z % suggestion.WorkgroupSize.z);
							ImGui::Text("SV_DispatchThreadID -> uint3(%d, %d, %d)", thread.x, thread.y, thread.z);
							ImGui::Text("SV_GroupIndex -> %d", localInvocationIndex);
						} else {
							ImGui::Text("gl_NumWorkGroups -> uvec3(%d, %d, %d)", pass->WorkX, pass->WorkY, pass->WorkZ);
							ImGui::Text("gl_WorkGroupID -> uvec3(%d, %d, %d)", thread.x / suggestion.WorkgroupSize.x, thread.y / suggestion.WorkgroupSize.y, thread.z / suggestion.WorkgroupSize.z);
							ImGui::Text("gl_LocalInvocationID -> uvec3(%d, %d, %d)", thread.x % suggestion.WorkgroupSize.x, thread.y % suggestion.WorkgroupSize.y, thread.z % suggestion.WorkgroupSize.z);
							ImGui::Text("gl_GlobalInvocationID -> uvec3(%d, %d, %d)", thread.x, thread.y, thread.z);
							ImGui::Text("gl_LocalInvocationIndex -> %d", localInvocationIndex);
						}
					}
					m_suggestionHeights[sugId] = (ImGui::GetCursorPosY() - pxCursorStart) + 2 * ImGui::GetStyle().WindowPadding.y;
				}
				ImGui::EndChild();
				ImGui::NewLine();

				ImGui::PopID();
				sugId++;
			}
		
			ImGui::PopStyleColor();
		}

		ImGui::PopStyleColor();
		ImGui::EndChild();
	}
	void PixelInspectUI::RenderPixelInfo(PixelInformation& pixel, float& elementHeight)
	{
		if (ImGui::BeginChild("##pixel_container", ImVec2(0, elementHeight), true)) {
			float pxCursorStart = ImGui::GetCursorPosY();

			/* [PASS NAME, RT NAME, OBJECT NAME, COORDINATE] */
			ImGui::Text("%s(%s) - %s@(%d,%d)", pixel.Pass->Name, pixel.RenderTexture == nullptr ? "Window" : pixel.RenderTexture->Name.c_str(), pixel.Object->Name, pixel.Coordinate.x, pixel.Coordinate.y);

			/* [PIXEL COLOR] */
			ImGui::PushItemWidth(-1);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::ColorEdit4("##pixel_edit", const_cast<float*>(glm::value_ptr(pixel.Color)));
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();

			if (!pixel.Fetched) {
				if (ImGui::Button("Fetch##pixel_fetch", ImVec2(-1, 0))
					&& m_data->Messages.CanRenderPreview()) {
					m_data->FetchPixel(pixel);
				}
			} else {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

				PluginShaderEditor pluginEditor;
				TextEditor* editor = nullptr;
				bool requestCompile = false;
				int initIndex = 0;

				bool vertexShaderEnabled = true, pixelShaderEnabled = true;
				if (pixel.Pass->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* plData = (pipe::PluginItemData*)pixel.Pass->Data;
					vertexShaderEnabled = plData->Owner->PipelineItem_IsStageDebuggable(plData->Type, plData->PluginData, ed::plugin::ShaderStage::Vertex);
					pixelShaderEnabled = plData->Owner->PipelineItem_IsStageDebuggable(plData->Type, plData->PluginData, ed::plugin::ShaderStage::Pixel);
				}

				/* [PIXEL] */
				if (!pixelShaderEnabled) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}
				if (ImGui::Button(UI_ICON_PLAY "##debug_pixel", ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))
					&& m_data->Messages.CanRenderPreview()) {
					pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Pass->Data);

					CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
					codeUI->StopDebugging();
					if (pixel.Pass->Type == PipelineItem::ItemType::ShaderPass)
						codeUI->Open(pixel.Pass, ShaderStage::Pixel);
					else if (pixel.Pass->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* plData = ((pipe::PluginItemData*)pixel.Pass->Data);
						plData->Owner->PipelineItem_OpenInEditor(plData->Type, plData->PluginData);
					}
					editor = codeUI->Get(pixel.Pass, ShaderStage::Pixel);

					// for plugins that store vertex and pixel shader in the same file
					if (editor == nullptr && pixel.Pass->Type == PipelineItem::ItemType::PluginItem)
						editor = codeUI->Get(pixel.Pass, ShaderStage::Vertex);
					if (editor == nullptr)
						pluginEditor = codeUI->GetPluginEditor(pixel.Pass, ShaderStage::Pixel);

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
				if (!pixelShaderEnabled) {
					ImGui::PopStyleVar();
					ImGui::PopItemFlag();
				}

				/* [VERTEX] */
				if (!vertexShaderEnabled) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}
				for (int i = 0; i < pixel.VertexCount; i++) {
					ImGui::PushID(i);
					if (ImGui::Button(UI_ICON_PLAY "##debug_vertex", ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))
						&& m_data->Messages.CanRenderPreview()) {
						CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
						codeUI->StopDebugging();
						if (pixel.Pass->Type == PipelineItem::ItemType::ShaderPass)
							codeUI->Open(pixel.Pass, ShaderStage::Vertex);
						else if (pixel.Pass->Type == PipelineItem::ItemType::PluginItem) {
							pipe::PluginItemData* plData = ((pipe::PluginItemData*)pixel.Pass->Data);
							plData->Owner->PipelineItem_OpenInEditor(plData->Type, plData->PluginData);
						}
						editor = codeUI->Get(pixel.Pass, ShaderStage::Vertex);

						if (editor == nullptr)
							pluginEditor = codeUI->GetPluginEditor(pixel.Pass, ShaderStage::Vertex);

						m_data->Debugger.PrepareVertexShader(pixel.Pass, pixel.Object);
						m_data->Debugger.SetVertexShaderInput(pixel, i);

						initIndex = i;
						requestCompile = true;
					}
					ImGui::PopID();
					ImGui::SameLine();
					ImGui::Text("Vertex[%d] = (%.2f, %.2f, %.2f)", i, pixel.Vertex[i].Position.x, pixel.Vertex[i].Position.y, pixel.Vertex[i].Position.z);
				}
				if (!vertexShaderEnabled) {
					ImGui::PopStyleVar();
					ImGui::PopItemFlag();
				}

				/* [GEOMETRY SHADER] */
				if (pixel.GeometryShaderUsed) {
					if (ImGui::Button(UI_ICON_PLAY "##debug_geometryshader", ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)) && m_data->Messages.CanRenderPreview()) {
						CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
						codeUI->StopDebugging();
						codeUI->Open(pixel.Pass, ShaderStage::Geometry);
						editor = codeUI->Get(pixel.Pass, ShaderStage::Geometry);

						if (editor == nullptr)
							pluginEditor = codeUI->GetPluginEditor(pixel.Pass, ShaderStage::Geometry);

						m_data->Debugger.PrepareGeometryShader(pixel.Pass, pixel.Object);
						m_data->Debugger.SetGeometryShaderInput(pixel);

						requestCompile = true;
					}
					ImGui::SameLine();
					ImGui::Text("Geometry Shader");
				}

				/* [TESSELLATION SHADER] */
				if (pixel.TessellationShaderUsed) {
					if (ImGui::Button(UI_ICON_PLAY "##debug_tessctrlshader", ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)) && m_data->Messages.CanRenderPreview()) {
						CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
						codeUI->StopDebugging();
						codeUI->Open(pixel.Pass, ShaderStage::TessellationControl);
						editor = codeUI->Get(pixel.Pass, ShaderStage::TessellationControl);

						if (editor == nullptr)
							pluginEditor = codeUI->GetPluginEditor(pixel.Pass, ShaderStage::TessellationControl);

						m_data->Debugger.PrepareTessellationControlShader(pixel.Pass, pixel.Object);
						m_data->Debugger.SetTessellationControlShaderInput(pixel);

						requestCompile = true;
					}
					ImGui::SameLine();
					ImGui::Text("Tessellation Control Shader");
				}

				/* ACTUAL ACTION HERE */
				if (requestCompile)
					StartDebugging(editor, pluginEditor, &pixel);

				ImGui::PopStyleColor();
			}

			elementHeight = (ImGui::GetCursorPosY() - pxCursorStart) + 2 * ImGui::GetStyle().WindowPadding.y;
		}
		ImGui::EndChild();
		ImGui::NewLine();
	}
	void PixelInspectUI::StartDebugging(TextEditor* editor, const PluginShaderEditor& pluginEditor, PixelInformation* pixel)
	{
		Logger::Get().Log("Starting up the debugger");
		
		CodeEditorUI* codeEditor = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));

		std::string path = "";
		if (editor != nullptr)
			path = editor->GetPath();
		else
			path = codeEditor->GetPluginEditorPath(pluginEditor);

		
		m_data->Debugger.SetCurrentFile(path);
		m_data->Debugger.SetDebugging(true);
		m_data->Debugger.PrepareDebugger();

		// text editor stack
		m_editorStack.clear();
		m_editorStack.push_back(path);

		// start the DAP server
		m_data->DAP.StartDebugging(path, m_data->Debugger.GetStage());

		// skip initialization
		if (editor != nullptr)
			editor->SetCurrentLineIndicator(m_data->Debugger.GetCurrentLine());
		else if (pluginEditor.Plugin->GetVersion() >= 3)
			((IPlugin3*)pluginEditor.Plugin)->ShaderEditor_SetLineIndicator(pluginEditor.LanguageID, pluginEditor.ID, m_data->Debugger.GetCurrentLine());

		if (pixel)
			m_data->Plugins.HandleApplicationEvent(ed::plugin::ApplicationEvent::DebuggerStarted, pixel->Pass->Name, editor != nullptr ? (void*)editor : (void*)&pluginEditor);

		// reset function stack
		((DebugFunctionStackUI*)m_ui->Get(ViewID::DebugFunctionStack))->Refresh();

		// update variable values
		((DebugValuesUI*)m_ui->Get(ViewID::DebugValues))->Refresh();

		// update watch values
		const std::vector<char*>& watchList = m_data->Debugger.GetWatchList();
		for (size_t i = 0; i < watchList.size(); i++)
			m_data->Debugger.UpdateWatchValue(i);

		// update vector watch values
		const std::vector<char*>& vectorWatchList = m_data->Debugger.GetVectorWatchList();
		for (size_t i = 0; i < vectorWatchList.size(); i++)
			m_data->Debugger.UpdateVectorWatchValue(i);

		// editor functions
		if (editor) {
			editor->OnDebuggerAction = [&](TextEditor* ed, TextEditor::DebugAction act) {
				CodeEditorUI* codeEditor = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));

				if (!m_data->Debugger.IsDebugging())
					return;

				m_cacheExpression.clear();

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

					// stop debugging in all editors
					for (int i = 0; i < m_editorStack.size(); i++) {
						if (codeEditor->Get(m_editorStack[i]))
							codeEditor->Get(m_editorStack[i])->SetCurrentLineIndicator(-1);
						else {
							PluginShaderEditor pluginEditor = codeEditor->GetPluginEditor(m_editorStack[i]);

							if (pluginEditor.Plugin->GetVersion() >= 3)
								((IPlugin3*)pluginEditor.Plugin)->ShaderEditor_SetLineIndicator(pluginEditor.LanguageID, pluginEditor.ID, -1);
						}
					}

					m_data->Plugins.HandleApplicationEvent(ed::plugin::ApplicationEvent::DebuggerStopped, nullptr, nullptr);
				} else {
					// update all the debug windows
					((DebugWatchUI*)m_ui->Get(ViewID::DebugWatch))->Refresh();
					((DebugVectorWatchUI*)m_ui->Get(ViewID::DebugVectorWatch))->Refresh();
					((DebugFunctionStackUI*)m_ui->Get(ViewID::DebugFunctionStack))->Refresh();
					((DebugValuesUI*)m_ui->Get(ViewID::DebugValues))->Refresh();
					if (m_data->Debugger.GetStage() == ShaderStage::TessellationControl)
						((DebugTessControlOutputUI*)m_ui->Get(ViewID::DebugTessControlOutput))->Refresh();

					int curLine = m_data->Debugger.GetCurrentLine();
					bool vmRunning = m_data->Debugger.IsVMRunning();

					// get filename
					std::string curFile = m_data->Debugger.GetVM()->current_file ? m_data->Debugger.GetVM()->current_file : "";
					if (curFile.empty() || curFile == "src/lib.rs" || curFile == "src\\lib.rs") // hack for PluginRust..
						curFile = m_data->Debugger.GetCurrentFile();
					else if (!std::filesystem::path(curFile).is_absolute())
						curFile = m_data->Parser.GetProjectPath(curFile);

					// open the file if it's not already open
					if (codeEditor->Get(curFile) == nullptr)
						codeEditor->OpenFile(curFile);

					TextEditor* actualEditor = codeEditor->Get(curFile);

					// stop debugging in all editors
					bool deleteStack = false;
					for (int i = 0; i < m_editorStack.size(); i++) {
						if (deleteStack || (i > 0 && !vmRunning)) {
							if (codeEditor->Get(m_editorStack[i]))
								codeEditor->Get(m_editorStack[i])->SetCurrentLineIndicator(-1);
							m_editorStack.erase(m_editorStack.begin() + i);
							i--;
						} else if (m_editorStack[i] == curFile)
							deleteStack = true;
					}
					if (!deleteStack)
						m_editorStack.push_back(curFile);

					// generate "Auto" watches
					DebugAutoUI* autoWnd = ((DebugAutoUI*)m_ui->Get(ViewID::DebugAuto));
					if (autoWnd->Visible)
						autoWnd->SetExpressions(actualEditor->GetRelevantExpressions(curLine));

					// update the line indicator
					if (!vmRunning) {
						curLine = ed->GetCurrentLineIndicator() + 1;
						ed->SetCurrentLineIndicator(curLine);
					} else
						actualEditor->SetCurrentLineIndicator(curLine, actualEditor == ed);

					// copy the debug methods if we are in some other file
					if (actualEditor != ed) {
						actualEditor->HasIdentifierHover = ed->HasIdentifierHover;
						actualEditor->OnIdentifierHover = ed->OnIdentifierHover;
						actualEditor->HasExpressionHover = ed->HasExpressionHover;
						actualEditor->OnExpressionHover = ed->OnExpressionHover;
					}

					m_data->Plugins.HandleApplicationEvent(ed::plugin::ApplicationEvent::DebuggerStepped, (void*)curLine, nullptr);
				}
			};
			editor->OnDebuggerJump = [&](TextEditor* ed, int line) {
				if (!m_data->Debugger.IsDebugging())
					return;

				m_cacheExpression.clear();

				while (m_data->Debugger.IsVMRunning() && m_data->Debugger.GetCurrentLine() != line)
					spvm_state_step_into(m_data->Debugger.GetVM());

				ed->SetCurrentLineIndicator(m_data->Debugger.GetCurrentLine());
				((DebugWatchUI*)m_ui->Get(ViewID::DebugWatch))->Refresh();
				((DebugVectorWatchUI*)m_ui->Get(ViewID::DebugVectorWatch))->Refresh();
				((DebugFunctionStackUI*)m_ui->Get(ViewID::DebugFunctionStack))->Refresh();
				((DebugValuesUI*)m_ui->Get(ViewID::DebugValues))->Refresh();
				if (m_data->Debugger.GetStage() == ShaderStage::TessellationControl)
					((DebugTessControlOutputUI*)m_ui->Get(ViewID::DebugTessControlOutput))->Refresh();
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
						 isCube = false,
						 isTex3D = false;

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

						if (resType) {
							spvm_image_info* image_info = resType->image_info;

							if (image_info == nullptr && m_data->Debugger.GetVM()) {
								spvm_result_t type_info = spvm_state_get_type_info(m_data->Debugger.GetVM()->results, resType);
								image_info = type_info->image_info;

								if (image_info == NULL) {
									type_info = &m_data->Debugger.GetVM()->results[type_info->pointer];
									image_info = type_info->image_info;
								}
							}

							if (image_info && image_info->dim == SpvDimCube)
								isCube = true;
							else if (image_info && image_info->dim == SpvDim3D)
								isTex3D = true;
						}
					}

					ImGui::Separator();

					// Value:
					if (isTex && res->image_data != nullptr) {
						spvm_image_t tex = res->image_data;

						if (isCube) {
							m_cubePrev.Draw((GLuint)((uintptr_t)tex->user_data));
							ImGui::Image((ImTextureID)m_cubePrev.GetTexture(), ImVec2(128.0f, 128.0f * (375.0f / 512.0f)), ImVec2(0, 1), ImVec2(1, 0));
						} else if (isTex3D) {
							float imgWH = (tex->height / (float)tex->width);
							m_tex3DPrev.Draw((GLuint)((uintptr_t)tex->user_data), 128.0f, 128.0f * (float)imgWH);
							ImGui::Image((void*)(intptr_t)m_tex3DPrev.GetTexture(), ImVec2(128.0f, 128.0f * imgWH));
						} else
							ImGui::Image((ImTextureID)tex->user_data, ImVec2(128.0f, 128.0f * (tex->height / (float)tex->width)), ImVec2(0, 1), ImVec2(1, 0));
					} else {
						// color preview
						if (resType->value_type == spvm_value_type_vector && m_data->Debugger.GetVM()->results[resType->pointer].value_type == spvm_value_type_float) {
							if (resType->member_count == 3) {
								float colorVal[3] = { res[0].value.f, res[1].value.f, res[2].value.f };
								ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
								ImGui::ColorEdit3("##value_preview", colorVal, ImGuiColorEditFlags_NoInputs);
								ImGui::PopItemFlag();
								ImGui::SameLine();
							} else if (resType->member_count == 4) {
								float colorVal[4] = { res[0].value.f, res[1].value.f, res[2].value.f, res[3].value.f };
								ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
								ImGui::ColorEdit4("##value_preview", colorVal, ImGuiColorEditFlags_NoInputs);
								ImGui::PopItemFlag();
								ImGui::SameLine();
							}
						}

						// text value
						std::stringstream ss;
						m_data->Debugger.GetVariableValueAsString(ss, m_data->Debugger.GetVM(), resType, res, vcount, "");
						ImGui::Text(ss.str().c_str());
					}
				}
			};
			editor->HasExpressionHover = [&](TextEditor* ed, const std::string& id) -> bool {
				if (!m_data->Debugger.IsDebugging() || !Settings::Instance().Debug.ShowValuesOnHover)
					return false;

				if (id == m_cacheExpression)
					return m_cacheExists;

				m_cacheExpression = id;

				spvm_result_t resType = nullptr;
				spvm_result_t exprVal = m_data->Debugger.Immediate(id, resType);

				m_cacheExists = exprVal != nullptr && resType != nullptr;

				if (m_cacheExists) {
					std::stringstream ss;
					m_data->Debugger.GetVariableValueAsString(ss, m_data->Debugger.GetVMImmediate(), resType, exprVal->members, exprVal->member_count, "");

					m_cacheValue = ss.str();
					m_cacheHasColor = false;
					m_cacheColor = glm::vec4(0.0f);

					if (resType->value_type == spvm_value_type_vector && m_data->Debugger.GetVMImmediate()->results[resType->pointer].value_type == spvm_value_type_float) {
						if (resType->member_count == 3)
							m_cacheHasColor = true;
						else if (resType->member_count == 4)
							m_cacheHasColor = true;
						for (int i = 0; i < resType->member_count; i++)
							m_cacheColor[i] = exprVal->members[i].value.f;
					}
				}

				return m_cacheExists;
			};
			editor->OnExpressionHover = [&](TextEditor* ed, const std::string& id) {
				ImGui::Text(id.c_str());
				ImGui::Separator();
				if (m_cacheHasColor) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::ColorEdit4("##value_preview", const_cast<float*>(glm::value_ptr(m_cacheColor)), ImGuiColorEditFlags_NoInputs);
					ImGui::PopItemFlag();
					ImGui::SameLine();
				}
				ImGui::Text(m_cacheValue.c_str());
			};
		}

		// copy preview camera info to vertex watch camera & geometry shader output camera
		if (!Settings::Instance().Project.FPCamera) {
			ArcBallCamera* previewCamera = (ArcBallCamera*)SystemVariableManager::Instance().GetCamera();
			
			DebugVectorWatchUI* vectorWatchUI = (DebugVectorWatchUI*)m_ui->Get(ViewID::DebugVectorWatch);
			ArcBallCamera* vectorCamera = vectorWatchUI->GetCamera();

			DebugGeometryOutputUI* geometryOutputUI = (DebugGeometryOutputUI*)m_ui->Get(ViewID::DebugGeometryOutput);
			ArcBallCamera* geometryCamera = geometryOutputUI->GetCamera();

			glm::vec3 rota = previewCamera->GetRotation();

			vectorCamera->SetDistance(previewCamera->GetDistance());
			vectorCamera->SetPitch(rota.x);
			vectorCamera->SetYaw(rota.y);
			vectorCamera->SetRoll(rota.z);

			geometryCamera->SetDistance(previewCamera->GetDistance());
			geometryCamera->SetPitch(rota.x);
			geometryCamera->SetYaw(rota.y);
			geometryCamera->SetRoll(rota.z);
		}
	}
}