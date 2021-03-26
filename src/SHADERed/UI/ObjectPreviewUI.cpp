#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/UI/Tools/DebuggerOutline.h>
#include <SHADERed/UI/ObjectPreviewUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <imgui/imgui.h>

#include <misc/ImFileDialog.h>

namespace ed {
	void ObjectPreviewUI::Open(ObjectManagerItem* item)
	{
		ed::Logger::Get().Log("Opening preview for \"" + item->Name + "\"");

		for (int i = 0; i < m_items.size(); i++) {
			if (m_isOpen[i] && item == m_items[i])
				return;
		}

		std::vector<ShaderVariable::ValueType> cachedFormat;
		int cachedSize = 0;
		if (item->Type == ObjectType::Buffer) {
			BufferObject* buf = item->Buffer;
			cachedFormat = m_data->Objects.ParseBufferFormat(buf->ViewFormat);
			cachedSize = buf->Size;
		}

		
		glm::vec2 imgSize(0, 0);
		if (item->Type == ObjectType::RenderTexture) {
			glm::ivec2 rtSize = m_data->Objects.GetRenderTextureSize(item);
			imgSize = glm::vec2(rtSize.x, rtSize.y);
		} else if (item->Type == ObjectType::Audio)
			imgSize = glm::vec2(512, 2);
		else if (item->Type == ObjectType::CubeMap)
			imgSize = glm::vec2(512, 375);
		else if (item->Type == ObjectType::Image)
			imgSize = item->Image->Size;
		else if (item->Type == ObjectType::Image3D)
			imgSize = item->Image3D->Size;
		else if (item->Type != ObjectType::PluginObject)
			imgSize = glm::vec2(item->TextureSize);
		else if (item->Type == ObjectType::Texture3D)
			imgSize = item->TextureSize;

		m_items.push_back(item);
		m_isOpen.push_back(true);
		m_cachedBufFormat.push_back(cachedFormat);
		m_cachedBufSize.push_back(cachedSize);
		m_cachedImgSize.push_back(imgSize);
		m_cachedImgSlice.push_back(0);
		m_zoom.push_back(Magnifier());
		m_zoomColor.push_back(0);
		m_zoomDepth.push_back(0);
		m_zoomFBO.push_back(0);
		m_lastRTSize.push_back(glm::vec2(0.0f, 0.0f));
	}
	void ObjectPreviewUI::OnEvent(const SDL_Event& e)
	{
		if (m_curHoveredItem != -1) {
			if (e.type == SDL_MOUSEBUTTONDOWN) {
				const Uint8* keyState = SDL_GetKeyboardState(NULL);
				bool isAltDown = keyState[SDL_SCANCODE_LALT] || keyState[SDL_SCANCODE_RALT];

				if (isAltDown) {
					if (e.button.button == SDL_BUTTON_LEFT)
						m_zoom[m_curHoveredItem].StartMouseAction(true);
					if (e.button.button == SDL_BUTTON_RIGHT)
						m_zoom[m_curHoveredItem].StartMouseAction(false);
				}
			} else if (e.type == SDL_MOUSEMOTION)
				m_zoom[m_curHoveredItem].Drag();
		}

		if (e.type == SDL_MOUSEBUTTONUP)
			for (int i = 0; i < m_items.size(); i++)
				m_zoom[i].EndMouseAction();
	}
	void ObjectPreviewUI::Update(float delta)
	{
		m_curHoveredItem = -1;

		for (int i = 0; i < m_items.size(); i++) {
			ObjectManagerItem* item = m_items[i];

			if (!m_isOpen[i])
				continue;

			std::string& name = item->Name;
			m_zoom[i].SetCurrentMousePosition(SystemVariableManager::Instance().GetMousePosition());

			if (ImGui::Begin((name + "###objprev" + std::to_string(i)).c_str(), (bool*)&m_isOpen[i])) {
				ImVec2 aSize = ImGui::GetContentRegionAvail();

				if (item->Plugin != nullptr) {
					PluginObject* pobj = ((PluginObject*)item->Plugin);
					pobj->Owner->Object_ShowExtendedPreview(pobj->Type, pobj->Data, pobj->ID);
				} else {
					glm::ivec2 iSize(m_cachedImgSize[i]);
					if (item->Type == ObjectType::RenderTexture) {
						ObjectManagerItem* actualData = m_data->Objects.Get(item->Name);

						iSize = m_data->Objects.GetRenderTextureSize(actualData);
					}

					float scale = std::min<float>(aSize.x / iSize.x, aSize.y / iSize.y);
					aSize.x = iSize.x * scale;
					aSize.y = iSize.y * scale;

					if (item->Type == ObjectType::CubeMap) {
						ImVec2 posSize = ImGui::GetContentRegionAvail();
						float posX = (posSize.x - aSize.x) / 2;
						float posY = (posSize.y - aSize.y) / 2;
						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY);

						glm::vec2 mousePos = glm::vec2((ImGui::GetMousePos().x - ImGui::GetCursorScreenPos().x) / aSize.x,
							1.0f - (ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y) / aSize.y);
						m_zoom[i].SetCurrentMousePosition(mousePos);

						const glm::vec2& zPos = m_zoom[i].GetZoomPosition();
						const glm::vec2& zSize = m_zoom[i].GetZoomSize();
						m_cubePrev.Draw(item->Texture);
						ImGui::Image((void*)(intptr_t)m_cubePrev.GetTexture(), aSize, ImVec2(zPos.x, zPos.y + zSize.y), ImVec2(zPos.x + zSize.x, zPos.y));

						if (ImGui::IsItemHovered()) m_curHoveredItem = i;

						if (m_curHoveredItem == i && ImGui::GetIO().KeyAlt && ImGui::IsMouseDoubleClicked(0))
							m_zoom[i].Reset();

						m_renderZoom(i, glm::vec2(aSize.x, aSize.y));

						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY);
						ImGui::Image((void*)(intptr_t)m_zoomColor[i], aSize, ImVec2(zPos.x, zPos.y + zSize.y), ImVec2(zPos.x + zSize.x, zPos.y));
					}
					else if (item->Type == ObjectType::Texture3D || item->Type == ObjectType::Image3D) {
						ImVec2 posSize = ImGui::GetContentRegionAvail();
						float posX = (posSize.x - aSize.x) / 2;
						float posY = (posSize.y - aSize.y) / 2;
						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY);

						glm::ivec3 objSize = glm::ivec3(item->TextureSize, item->Depth);
						if (item->Type == ObjectType::Image3D && item->Image3D)
							objSize = item->Image3D->Size;

						glm::vec2 mousePos = glm::vec2((ImGui::GetMousePos().x - ImGui::GetCursorScreenPos().x) / aSize.x,
							1.0f - (ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y) / aSize.y);
						m_zoom[i].SetCurrentMousePosition(mousePos);

						const glm::vec2& zPos = m_zoom[i].GetZoomPosition();
						const glm::vec2& zSize = m_zoom[i].GetZoomSize();
						m_tex3DPrev.Draw(item->Texture, objSize.x, objSize.y, (float)(m_cachedImgSlice[i] + 0.5f) / objSize.z);
						ImGui::Image((void*)(intptr_t)m_tex3DPrev.GetTexture(), aSize, ImVec2(zPos.x, zPos.y), ImVec2(zPos.x + zSize.x, zPos.y + zSize.y));

						if (ImGui::IsItemHovered()) m_curHoveredItem = i;

						if (m_curHoveredItem == i && ImGui::GetIO().KeyAlt && ImGui::IsMouseDoubleClicked(0))
							m_zoom[i].Reset();

						m_renderZoom(i, glm::vec2(aSize.x, aSize.y));

						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY);
						ImGui::Image((void*)(intptr_t)m_zoomColor[i], aSize, ImVec2(zPos.x, zPos.y), ImVec2(zPos.x + zSize.x, zPos.y + zSize.y));
					
						
						// slices
						posSize = ImGui::GetWindowContentRegionMax();
						ImGui::SetCursorPosY(posSize.y - Settings::Instance().CalculateSize(Settings::Instance().General.FontSize + 5.0f));

						ImGui::Text("%dx%dx%d", objSize.x, objSize.y, objSize.z);
						ImGui::SameLine();

						ImGui::Text("Slice: ");
						ImGui::SameLine();
						if (ImGui::Button("-", ImVec2(30, 0)))
							m_cachedImgSlice[i] = MAX(m_cachedImgSlice[i] - 1, 0);
						ImGui::SameLine();
						ImGui::Text("%d", m_cachedImgSlice[i]);
						ImGui::SameLine();
						if (ImGui::Button("+", ImVec2(30, 0)))
							m_cachedImgSlice[i] = MIN(m_cachedImgSlice[i] + 1, objSize.z - 1);
					}
					else if (item->Type == ObjectType::Audio) {
						memset(&m_samplesTempBuffer, 0, sizeof(short) * 1024);
						item->Sound->GetSamples(m_samplesTempBuffer);
						double* fftData = m_audioAnalyzer.FFT(m_samplesTempBuffer);

						for (int i = 0; i < ed::AudioAnalyzer::SampleCount; i++) {
							short s = (m_samplesTempBuffer[i * 2] + m_samplesTempBuffer[i * 2 + 1]) / 2;
							float sf = (float)s / (float)INT16_MAX;

							m_fft[i] = fftData[i / 2];
							m_samples[i] = sf * 0.5f + 0.5f;
						}

						ImGui::PlotHistogram("Frequencies", m_fft, IM_ARRAYSIZE(m_fft), 0, NULL, 0.0f, 1.0f, ImVec2(0, 80));
						ImGui::PlotHistogram("Samples", m_samples, IM_ARRAYSIZE(m_samples), 0, NULL, 0.0f, 1.0f, ImVec2(0, 80));
					} 
					else if (item->Type == ObjectType::Buffer) {
						BufferObject* buf = (BufferObject*)item->Buffer;

						ImGui::Text("Format:");
						ImGui::SameLine();
						if (ImGui::InputText("##objprev_formatinp", buf->ViewFormat, 256))
							m_data->Parser.ModifyProject();
						ImGui::SameLine();
						if (ImGui::Button("APPLY##objprev_applyfmt"))
							m_cachedBufFormat[i] = m_data->Objects.ParseBufferFormat(buf->ViewFormat);

						int perRow = 0;
						for (int j = 0; j < m_cachedBufFormat[i].size(); j++)
							perRow += ShaderVariable::GetSize(m_cachedBufFormat[i][j], true);
						ImGui::Text("Size per row: %d bytes", perRow);
						ImGui::Text("Total size: %d bytes", buf->Size);

						ImGui::Text("New buffer size (in bytes):");
						ImGui::SameLine();
						ImGui::PushItemWidth(200);
						ImGui::InputInt("##objprev_newsize", &m_cachedBufSize[i], 1, 10, ImGuiInputTextFlags_AlwaysInsertMode);
						ImGui::PopItemWidth();
						ImGui::SameLine();
						if (ImGui::Button("APPLY##objprev_applysize")) {
							int oldSize = buf->Size;
							
							buf->Size = m_cachedBufSize[i];
							if (buf->Size < 0) buf->Size = 0;

							void* newData = calloc(1, buf->Size);
							memcpy(newData, buf->Data, std::min<int>(oldSize, buf->Size));
							free(buf->Data);
							buf->Data = newData;

							glBindBuffer(GL_UNIFORM_BUFFER, buf->ID);
							glBufferData(GL_UNIFORM_BUFFER, buf->Size, buf->Data, GL_STATIC_DRAW); // resize
							glBindBuffer(GL_UNIFORM_BUFFER, 0);

							m_data->Parser.ModifyProject();
						}

						ImGui::Separator();
						ImGui::Text("Controls: ");

						if (ImGui::Button("CLEAR##objprev_clearbuf")) {
							memset(buf->Data, 0, buf->Size);

							glBindBuffer(GL_UNIFORM_BUFFER, buf->ID);
							glBufferData(GL_UNIFORM_BUFFER, buf->Size, buf->Data, GL_STATIC_DRAW); // upload data
							glBindBuffer(GL_UNIFORM_BUFFER, 0);

							m_data->Parser.ModifyProject();
						}
						ImGui::SameLine();
						if (ImGui::Button(buf->PreviewPaused  ? "UNPAUSE##objprev_unpause" : "PAUSE##objprev_pause"))
							buf->PreviewPaused = !buf->PreviewPaused;
						ImGui::SameLine();
						if (ImGui::Button("LOAD BYTE DATA FROM TEXTURE")) {
							m_dialogActionType = 0;
							ifd::FileDialog::Instance().Open("LoadObjectDlg", "Select a texture", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds){.png,.jpg,.jpeg,.bmp,.tga,.dds},.*");
						}
						ImGui::SameLine();
						if (ImGui::Button("LOAD FLOAT DATA FROM TEXTURE")) {
							m_dialogActionType = 1;
							ifd::FileDialog::Instance().Open("LoadObjectDlg", "Select a texture", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds){.png,.jpg,.jpeg,.bmp,.tga,.dds},.*");
						}
						if (ImGui::Button("LOAD DATA FROM 3D MODEL")) {
							m_dialogActionType = 2;
							ifd::FileDialog::Instance().Open("LoadObjectDlg", "3D model", ".*");
						}
						ImGui::SameLine();
						if (ImGui::Button("LOAD RAW DATA")) {
							m_dialogActionType = 3;
							ifd::FileDialog::Instance().Open("LoadObjectDlg", "Open", ".*");
						}

						if (ifd::FileDialog::Instance().IsDone("LoadObjectDlg")) {
							if (ifd::FileDialog::Instance().HasResult()) {
								std::string file = ifd::FileDialog::Instance().GetResult().u8string();

								if (m_dialogActionType == 0)
									m_data->Objects.LoadBufferFromTexture(buf, file);
								else if (m_dialogActionType == 1)
									m_data->Objects.LoadBufferFromTexture(buf, file, true);
								else if (m_dialogActionType == 2)
									m_data->Objects.LoadBufferFromModel(buf, file);
								else if (m_dialogActionType == 3)
									m_data->Objects.LoadBufferFromFile(buf, file);
							}
							ifd::FileDialog::Instance().Close();
						}

						ImGui::Separator();

						// update buffer data every 350ms
						ImGui::Text(buf->PreviewPaused ? "Buffer view is paused" : "Buffer view is updated every 350ms");
						if (!buf->PreviewPaused && m_bufUpdateClock.GetElapsedTime() > 0.350f && buf->Data != nullptr) {
							glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf->ID);
							glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, buf->Size, buf->Data);
							glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
							m_bufUpdateClock.Restart();
						}

						if (perRow != 0) {
							ImGui::Separator();

							int rows = buf->Size / perRow;
							float offsetY = ImGui::GetCursorPosY();
							float scrollY = std::max<float>(0.0f, ImGui::GetScrollY() - offsetY);
							float yAdvance = ImGui::GetTextLineHeightWithSpacing() + 2 * ImGui::GetStyle().FramePadding.y + 2.0f;
							ImVec2 contentSize = ImGui::GetWindowContentRegionMax();

							ImGui::BeginChild("##buf_container", ImVec2(0, (rows + 1) * yAdvance));

							ImGui::Columns(m_cachedBufFormat[i].size() + 1);
							if (!m_initRowSize) { // imgui hax
								ImGui::SetColumnWidth(0, Settings::Instance().CalculateSize(50.0f));
								m_initRowSize = true;
							}
							ImGui::Text("Row");
							ImGui::NextColumn();
							for (int j = 0; j < m_cachedBufFormat[i].size(); j++) {
								ImGui::Text(VARIABLE_TYPE_NAMES[(int)m_cachedBufFormat[i][j]]);
								ImGui::NextColumn();
							}
							ImGui::Separator();

							int rowNo = std::max<int>(0, (int)floor(scrollY / yAdvance) - 5);
							int rowMax = std::max<int>(0, std::min<int>((int)rows, rowNo + (int)floor((scrollY + contentSize.y + offsetY) / yAdvance) + 10));
							float cursorY = ImGui::GetCursorPosY();

							for (int j = rowNo; j < rowMax; j++) {
								ImGui::PushID(j);

								ImGui::SetCursorPosY(cursorY + j * yAdvance);
								ImGui::Text("%d", j+1);
								ImGui::NextColumn();
								int curColOffset = 0;
								for (int k = 0; k < m_cachedBufFormat[i].size(); k++) {
									ImGui::PushID(k);
									ImGui::SetCursorPosY(cursorY + j * yAdvance);

									int dOffset = j * perRow + curColOffset;
									if (m_drawBufferElement(j, k, (void*)(((char*)buf->Data) + dOffset), m_cachedBufFormat[i][k])) {
										glBindBuffer(GL_UNIFORM_BUFFER, buf->ID);
										glBufferData(GL_UNIFORM_BUFFER, buf->Size, buf->Data, GL_STATIC_DRAW); // allocate 0 bytes of memory
										glBindBuffer(GL_UNIFORM_BUFFER, 0);

										m_data->Parser.ModifyProject();
									}

									curColOffset += ShaderVariable::GetSize(m_cachedBufFormat[i][k], true);
									ImGui::NextColumn();
									ImGui::PopID();
								}

								ImGui::PopID();
							}

							ImGui::Columns(1);

							ImGui::EndChild();
						}

					} 
					else {
						ImVec2 posSize = ImGui::GetWindowContentRegionMax();
						ImVec2 test = ImGui::GetWindowContentRegionMin();
						float posX = (posSize.x - aSize.x + test.x) / 2;
						float posY = (posSize.y - aSize.y + test.y) / 2;
						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY);

						glm::vec2 mousePos = glm::vec2((ImGui::GetMousePos().x - ImGui::GetCursorScreenPos().x) / aSize.x,
							1.0f - (ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y) / aSize.y);
						m_zoom[i].SetCurrentMousePosition(mousePos);

						const glm::vec2& zPos = m_zoom[i].GetZoomPosition();
						const glm::vec2& zSize = m_zoom[i].GetZoomSize();
						
						ImGui::Image((void*)(intptr_t)item->Texture, aSize, ImVec2(zPos.x, zPos.y + zSize.y), ImVec2(zPos.x + zSize.x, zPos.y));

						if (ImGui::IsItemHovered()) {
							m_curHoveredItem = i;
						
							// handle pixel selection
							if (m_data->Renderer.IsPaused() && ((ImGui::IsMouseClicked(0) && !Settings::Instance().Preview.SwitchLeftRightClick) || (ImGui::IsMouseClicked(1) && Settings::Instance().Preview.SwitchLeftRightClick)) && !ImGui::GetIO().KeyAlt) {
								// render texture
								if (item->RT != nullptr) { 
									m_ui->StopDebugging();

									// screen space position
									glm::vec2 s(zPos.x + zSize.x * mousePos.x, zPos.y + zSize.y * mousePos.y);

									m_data->DebugClick(s);
								}
								// image
								ImageObject* image = m_data->Objects.Get(name)->Image;
								if (image != nullptr) {
									glm::vec2 s(zPos.x + zSize.x * mousePos.x, zPos.y + zSize.y * mousePos.y);

									for (auto& pipeItem : m_data->Pipeline.GetList()) {
										if (m_data->Objects.IsUniformBound(item, pipeItem) != -1) {
											if (pipeItem->Type == PipelineItem::ItemType::ComputePass) {
												DebuggerSuggestion suggestion;
												suggestion.Type = DebuggerSuggestion::SuggestionType::ComputeShader;
												suggestion.Item = pipeItem;
												suggestion.Thread = glm::ivec3(s.x * image->Size.x, s.y * image->Size.y, 0);
												m_data->Debugger.AddSuggestion(suggestion);
											}
										}
									}
								}
							}
						}

						if (!ImGui::GetIO().KeyAlt && ImGui::BeginPopupContextItem("##context")) {
							if (ImGui::Selectable("Save")) {
								ifd::FileDialog::Instance().Save("SavePreviewTextureDlg", "Save", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds){.png,.jpg,.jpeg,.bmp,.tga,.dds},.*");
								m_saveObject = item;
							}
							ImGui::EndPopup();
						}

						if (m_curHoveredItem == i && ImGui::GetIO().KeyAlt && ImGui::IsMouseDoubleClicked(0))
							m_zoom[i].Reset();
						
						m_renderZoom(i, glm::vec2(aSize.x, aSize.y));

						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY);
						ImGui::Image((void*)(intptr_t)m_zoomColor[i], aSize, ImVec2(zPos.x, zPos.y + zSize.y), ImVec2(zPos.x + zSize.x, zPos.y));

						// statusbar & debugger overlay
						if (item->Type == ObjectType::RenderTexture) {

							auto& pixelList = m_data->Debugger.GetPixelList();

							// debugger overlay
							if (pixelList.size() > 0 && (Settings::Instance().Debug.PixelOutline || Settings::Instance().Debug.PrimitiveOutline)) {
								unsigned int outlineColor = 0xffffffff;
								auto drawList = ImGui::GetWindowDrawList();
								static char pxCoord[32] = { 0 };

								for (int i = 0; i < pixelList.size(); i++) {
									if (pixelList[i].RenderTexture->Name == item->Name && pixelList[i].Fetched) { // we only care about window's pixel info here

										if (Settings::Instance().Debug.PrimitiveOutline) {
											ImGui::SetCursorPosX(posX);
											ImGui::SetCursorPosY(posY);

											ImVec2 uiPos = ImGui::GetCursorScreenPos();

											DebuggerOutline::RenderPrimitiveOutline(pixelList[i], glm::vec2(uiPos.x, uiPos.y), glm::vec2(aSize.x, aSize.y), zPos, zSize);
										}
										if (Settings::Instance().Debug.PixelOutline) {
											ImGui::SetCursorPosX(posX);
											ImGui::SetCursorPosY(posY);

											ImVec2 uiPos = ImGui::GetCursorScreenPos();

											DebuggerOutline::RenderPixelOutline(pixelList[i], glm::vec2(uiPos.x, uiPos.y), glm::vec2(aSize.x, aSize.y), zPos, zSize);
										}
									}
								}
							}
	
							
							// statusbar
							ImGui::SetCursorPosY(posSize.y - Settings::Instance().CalculateSize(25));

							ImGui::Text("%dx%d", iSize.x, iSize.y);
							ImGui::SameLine();

							ImGui::SameLine(Settings::Instance().CalculateSize(75));
							ImGui::Text("FMT: %s", gl::String::Format(((ed::RenderTextureObject*)item->RT)->Format));
							ImGui::SameLine();
						}
					}
				}
			}
			ImGui::End();
		}

		for (int i = 0; i < m_items.size(); i++) {
			if (!m_isOpen[i]) {
				Close(m_items[i]->Name);
				i--;
			}
		}

		if (ifd::FileDialog::Instance().IsDone("SavePreviewTextureDlg")) {
			if (ifd::FileDialog::Instance().HasResult() && m_saveObject)
				m_data->Objects.SaveToFile(m_saveObject, ifd::FileDialog::Instance().GetResult().u8string());
			ifd::FileDialog::Instance().Close();
		}
	}
	bool ObjectPreviewUI::m_drawBufferElement(int row, int col, void* data, ShaderVariable::ValueType type)
	{
		bool ret = false;

		ImGui::PushItemWidth(-1);

		switch (type) {
		case ed::ShaderVariable::ValueType::Float4x4:
		case ed::ShaderVariable::ValueType::Float3x3:
		case ed::ShaderVariable::ValueType::Float2x2: {
			int cols = 2;
			if (type == ShaderVariable::ValueType::Float3x3)
				cols = 3;
			else if (type == ShaderVariable::ValueType::Float4x4)
				cols = 4;

			for (int y = 0; y < cols; y++) {
				ImGui::PushID(y);

				if (type == ShaderVariable::ValueType::Float2x2)
					ret |= ImGui::DragFloat2("##valuedit", (float*)data, 0.01f);
				else if (type == ShaderVariable::ValueType::Float3x3)
					ret |= ImGui::DragFloat3("##valuedit", (float*)data, 0.01f);
				else
					ret |= ImGui::DragFloat4("##valuedit", (float*)data, 0.01f);

				ImGui::PopID();
			}
		} break;
		case ed::ShaderVariable::ValueType::Float1:
			ret |= ImGui::DragFloat("##valuedit", (float*)data, 0.01f);
			break;
		case ed::ShaderVariable::ValueType::Float2:
			ret |= ImGui::DragFloat2("##valuedit", (float*)data, 0.01f);
			break;
		case ed::ShaderVariable::ValueType::Float3:
			ret |= ImGui::DragFloat3("##valuedit", (float*)data, 0.01f);
			break;
		case ed::ShaderVariable::ValueType::Float4:
			ret |= ImGui::DragFloat4("##valuedit", (float*)data, 0.01f);
			break;
		case ed::ShaderVariable::ValueType::Integer1:
			ret |= ImGui::DragInt("##valuedit", (int*)data, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Integer2:
			ret |= ImGui::DragInt2("##valuedit", (int*)data, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Integer3:
			ret |= ImGui::DragInt3("##valuedit", (int*)data, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Integer4:
			ret |= ImGui::DragInt4("##valuedit", (int*)data, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Boolean1:
			ret |= ImGui::DragScalar("##valuedit", ImGuiDataType_U8, (int*)data, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Boolean2:
			ret |= ImGui::DragScalarN("##valuedit", ImGuiDataType_U8, (int*)data, 2, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Boolean3:
			ret |= ImGui::DragScalarN("##valuedit", ImGuiDataType_U8, (int*)data, 3, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Boolean4:
			ret |= ImGui::DragScalarN("##valuedit", ImGuiDataType_U8, (int*)data, 4, 1.0f);
			break;
		}

		ImGui::PopItemWidth();

		return ret;
	}
	void ObjectPreviewUI::m_renderZoom(int ind, glm::vec2 itemSize)
	{
		if (itemSize != m_lastRTSize[ind]) {
			m_lastRTSize[ind] = itemSize;
			gl::FreeSimpleFramebuffer(m_zoomFBO[ind], m_zoomColor[ind], m_zoomDepth[ind]);
			m_zoomFBO[ind] = gl::CreateSimpleFramebuffer(itemSize.x, itemSize.y, m_zoomColor[ind], m_zoomDepth[ind]);
		
			m_zoom[ind].RebuildVBO(itemSize.x, itemSize.y);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, m_zoomFBO[ind]);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glViewport(0, 0, itemSize.x, itemSize.y);

		if (m_zoom[ind].IsSelecting())
			m_zoom[ind].Render();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

	}
	void ObjectPreviewUI::CloseAll()
	{
		for (int i = 0; i < m_items.size(); i++) {
			Close(m_items[i]->Name);
			i--;
		}
	}
	void ObjectPreviewUI::Close(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (m_items[i]->Name == name) {
				// sheesh... what are objects, amirite?
				m_items.erase(m_items.begin() + i);
				m_isOpen.erase(m_isOpen.begin() + i);
				m_cachedBufFormat.erase(m_cachedBufFormat.begin() + i);
				m_cachedBufSize.erase(m_cachedBufSize.begin() + i);
				m_cachedImgSize.erase(m_cachedImgSize.begin() + i);
				m_cachedImgSlice.erase(m_cachedImgSlice.begin() + i);
				m_zoom.erase(m_zoom.begin() + i);
				m_zoomColor.erase(m_zoomColor.begin() + i);
				m_zoomDepth.erase(m_zoomDepth.begin() + i);
				m_zoomFBO.erase(m_zoomFBO.begin() + i);
				m_lastRTSize.erase(m_lastRTSize.begin() + i);
				i--;
			}
		}
	}
}