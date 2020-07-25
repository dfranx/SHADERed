#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/UI/Tools/DebuggerOutline.h>
#include <SHADERed/UI/ObjectPreviewUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <imgui/imgui.h>

#include <ImGuiFileDialog/ImGuiFileDialog.h>

namespace ed {
	void ObjectPreviewUI::Open(const std::string& name, float w, float h, unsigned int item, bool isCube, void* rt, void* audio, void* buffer, void* plugin)
	{
		for (int i = 0; i < m_items.size(); i++) {
			mItem* item = &m_items[i];
			if (item->IsOpen && item->Name == name)
				return;
		}

		mItem i;
		i.Name = name;
		i.Width = w;
		i.Height = h;
		i.Texture = item;
		i.IsOpen = true;
		i.IsCube = isCube;
		i.Audio = audio;
		i.RT = rt;
		i.Buffer = buffer;
		i.CachedFormat.clear();
		i.CachedSize = 0;
		i.Plugin = plugin;

		if (buffer != nullptr) {
			BufferObject* buf = (BufferObject*)buffer;
			i.CachedFormat = m_data->Objects.ParseBufferFormat(buf->ViewFormat);
			i.CachedSize = buf->Size;
		}

		m_items.push_back(i);
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
			mItem* item = &m_items[i];

			if (!item->IsOpen)
				continue;

			std::string& name = item->Name;
			m_zoom[i].SetCurrentMousePosition(SystemVariableManager::Instance().GetMousePosition());

			if (ImGui::Begin((name + "###objprev" + std::to_string(i)).c_str(), &item->IsOpen)) {
				ImVec2 aSize = ImGui::GetContentRegionAvail();

				if (item->Plugin != nullptr) {
					PluginObject* pobj = ((PluginObject*)item->Plugin);
					pobj->Owner->Object_ShowExtendedPreview(pobj->Type, pobj->Data, pobj->ID);
				} else {
					glm::ivec2 iSize(item->Width, item->Height);
					if (item->RT != nullptr)
						iSize = m_data->Objects.GetRenderTextureSize(name);

					float scale = std::min<float>(aSize.x / iSize.x, aSize.y / iSize.y);
					aSize.x = iSize.x * scale;
					aSize.y = iSize.y * scale;

					if (item->IsCube) {
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
					else if (item->Audio != nullptr) {
						sf::Sound* player = m_data->Objects.GetAudioPlayer(item->Name);
						sf::SoundBuffer* buffer = (sf::SoundBuffer*)item->Audio;
						int channels = buffer->getChannelCount();
						int perChannel = buffer->getSampleCount() / channels;
						int curSample = (int)((player->getPlayingOffset().asSeconds() / buffer->getDuration().asSeconds()) * perChannel);

						double* fftData = m_audioAnalyzer.FFT(*buffer, curSample);

						const sf::Int16* samples = buffer->getSamples();
						for (int i = 0; i < ed::AudioAnalyzer::SampleCount; i++) {
							sf::Int16 s = samples[std::min<int>(i + curSample, perChannel)];
							float sf = (float)s / (float)INT16_MAX;

							m_fft[i] = fftData[i / 2];
							m_samples[i] = sf * 0.5f + 0.5f;
						}

						ImGui::PlotHistogram("Frequencies", m_fft, IM_ARRAYSIZE(m_fft), 0, NULL, 0.0f, 1.0f, ImVec2(0, 80));
						ImGui::PlotHistogram("Samples", m_samples, IM_ARRAYSIZE(m_samples), 0, NULL, 0.0f, 1.0f, ImVec2(0, 80));
					} 
					else if (item->Buffer != nullptr) {
						BufferObject* buf = (BufferObject*)item->Buffer;

						ImGui::Text("Format:");
						ImGui::SameLine();
						if (ImGui::InputText("##objprev_formatinp", buf->ViewFormat, 256))
							m_data->Parser.ModifyProject();
						ImGui::SameLine();
						if (ImGui::Button("APPLY##objprev_applyfmt"))
							item->CachedFormat = m_data->Objects.ParseBufferFormat(buf->ViewFormat);

						int perRow = 0;
						for (int i = 0; i < item->CachedFormat.size(); i++)
							perRow += ShaderVariable::GetSize(item->CachedFormat[i], true);
						ImGui::Text("Size per row: %d bytes", perRow);
						ImGui::Text("Total size: %d bytes", buf->Size);

						ImGui::Text("New buffer size (in bytes):");
						ImGui::SameLine();
						ImGui::PushItemWidth(200);
						ImGui::InputInt("##objprev_newsize", &item->CachedSize, 1, 10, ImGuiInputTextFlags_AlwaysInsertMode);
						ImGui::PopItemWidth();
						ImGui::SameLine();
						if (ImGui::Button("APPLY##objprev_applysize")) {
							int oldSize = buf->Size;
							
							buf->Size = item->CachedSize;
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
							igfd::ImGuiFileDialog::Instance()->OpenModal("LoadObjectDlg", "Select a texture", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*", ".");
						}
						ImGui::SameLine();
						if (ImGui::Button("LOAD FLOAT DATA FROM TEXTURE")) {
							m_dialogActionType = 1;
							igfd::ImGuiFileDialog::Instance()->OpenModal("LoadObjectDlg", "Select a texture", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*", ".");
						}
						if (ImGui::Button("LOAD DATA FROM 3D MODEL")) {
							m_dialogActionType = 2;
							igfd::ImGuiFileDialog::Instance()->OpenModal("LoadObjectDlg", "3D model", ".*", ".");
						}
						ImGui::SameLine();
						if (ImGui::Button("LOAD RAW DATA")) {
							m_dialogActionType = 3;
							igfd::ImGuiFileDialog::Instance()->OpenModal("LoadObjectDlg", "Open", ".*", ".");
						}

						if (igfd::ImGuiFileDialog::Instance()->FileDialog("LoadObjectDlg")) {
							if (igfd::ImGuiFileDialog::Instance()->IsOk) {
								std::string file = igfd::ImGuiFileDialog::Instance()->GetFilepathName();

								if (m_dialogActionType == 0)
									m_data->Objects.LoadBufferFromTexture(buf, file);
								else if (m_dialogActionType == 1)
									m_data->Objects.LoadBufferFromTexture(buf, file, true);
								else if (m_dialogActionType == 2)
									m_data->Objects.LoadBufferFromModel(buf, file);
								else if (m_dialogActionType == 3)
									m_data->Objects.LoadBufferFromFile(buf, file);
							}
							igfd::ImGuiFileDialog::Instance()->CloseDialog("LoadObjectDlg");
						}

						ImGui::Separator();

						// update buffer data every 350ms
						ImGui::Text(buf->PreviewPaused ? "Buffer view is paused" : "Buffer view is updated every 350ms");
						if (!buf->PreviewPaused && m_bufUpdateClock.GetElapsedTime() > 0.350f) {
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

							ImGui::Columns(item->CachedFormat.size() + 1);
							if (!m_initRowSize) { // imgui hax
								ImGui::SetColumnWidth(0, Settings::Instance().CalculateSize(50.0f));
								m_initRowSize = true;
							}
							ImGui::Text("Row");
							ImGui::NextColumn();
							for (int j = 0; j < item->CachedFormat.size(); j++) {
								ImGui::Text(VARIABLE_TYPE_NAMES[(int)item->CachedFormat[j]]);
								ImGui::NextColumn();
							}
							ImGui::Separator();

							int rowNo = std::max<int>(0, (int)floor(scrollY / yAdvance) - 5);
							int rowMax = std::max<int>(0, std::min<int>((int)rows, rowNo + (int)floor((scrollY + contentSize.y + offsetY) / yAdvance) + 10));
							float cursorY = ImGui::GetCursorPosY();

							for (int i = rowNo; i < rowMax; i++) {
								ImGui::SetCursorPosY(cursorY + i * yAdvance);
								ImGui::Text("%d", i+1);
								ImGui::NextColumn();
								int curColOffset = 0;
								for (int j = 0; j < item->CachedFormat.size(); j++) {
									ImGui::SetCursorPosY(cursorY + i * yAdvance);

									int dOffset = i * perRow + curColOffset;
									if (m_drawBufferElement(i, j, (void*)(((char*)buf->Data) + dOffset), item->CachedFormat[j])) {
										glBindBuffer(GL_UNIFORM_BUFFER, buf->ID);
										glBufferData(GL_UNIFORM_BUFFER, buf->Size, buf->Data, GL_STATIC_DRAW); // allocate 0 bytes of memory
										glBindBuffer(GL_UNIFORM_BUFFER, 0);

										m_data->Parser.ModifyProject();
									}
									curColOffset += ShaderVariable::GetSize(item->CachedFormat[j], true);
									ImGui::NextColumn();
								}
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
							if (m_data->Renderer.IsPaused() && item->RT != nullptr && ((ImGui::IsMouseClicked(0) && !Settings::Instance().Preview.SwitchLeftRightClick) || (ImGui::IsMouseClicked(1) && Settings::Instance().Preview.SwitchLeftRightClick)) && !ImGui::GetIO().KeyAlt) {
								m_ui->StopDebugging();

								// screen space position
								glm::vec2 s(zPos.x + zSize.x * mousePos.x, zPos.y + zSize.y * mousePos.y);

								m_data->DebugClick(s);
							}
						}

						if (!ImGui::GetIO().KeyAlt && ImGui::BeginPopupContextItem("##context")) {
							if (ImGui::Selectable("Save")) {
								igfd::ImGuiFileDialog::Instance()->OpenModal("SavePreviewTextureDlg", "Save", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*", ".");
								m_saveObject = item->Name;
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
						if (item->RT != nullptr) {

							auto& pixelList = m_data->Debugger.GetPixelList();

							// debugger overlay
							if (pixelList.size() > 0 && (Settings::Instance().Debug.PixelOutline || Settings::Instance().Debug.PrimitiveOutline)) {
								unsigned int outlineColor = 0xffffffff;
								auto drawList = ImGui::GetWindowDrawList();
								static char pxCoord[32] = { 0 };

								for (int i = 0; i < pixelList.size(); i++) {
									if (pixelList[i].RenderTexture == item->Name && pixelList[i].Fetched) { // we only care about window's pixel info here

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

			if (!item->IsOpen) {
				m_items.erase(m_items.begin() + i);
				m_zoom.erase(m_zoom.begin() + i);
				m_zoomColor.erase(m_zoomColor.begin() + i);
				m_zoomDepth.erase(m_zoomDepth.begin() + i);
				m_zoomFBO.erase(m_zoomFBO.begin() + i);
				m_lastRTSize.erase(m_lastRTSize.begin() + i);
				i--;
			}
		}

		if (igfd::ImGuiFileDialog::Instance()->FileDialog("SavePreviewTextureDlg")) {
			if (igfd::ImGuiFileDialog::Instance()->IsOk) {
				std::string filePath = igfd::ImGuiFileDialog::Instance()->GetFilepathName();
				ObjectManagerItem* oItem = m_data->Objects.GetObjectManagerItem(m_saveObject);
				m_data->Objects.SaveToFile(m_saveObject, oItem, filePath);
			}
			igfd::ImGuiFileDialog::Instance()->CloseDialog("SavePreviewTextureDlg");
		}
	}
	bool ObjectPreviewUI::m_drawBufferElement(int row, int col, void* data, ShaderVariable::ValueType type)
	{
		bool ret = false;

		ImGui::PushItemWidth(-1);

		std::string id = std::to_string(row) + std::to_string(col);

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
				if (type == ShaderVariable::ValueType::Float2x2)
					ret |= ImGui::DragFloat2(("##valuedit" + id + std::to_string(y)).c_str(), (float*)data, 0.01f);
				else if (type == ShaderVariable::ValueType::Float3x3)
					ret |= ImGui::DragFloat3(("##valuedit" + id + std::to_string(y)).c_str(), (float*)data, 0.01f);
				else
					ret |= ImGui::DragFloat4(("##valuedit" + id + std::to_string(y)).c_str(), (float*)data, 0.01f);
			}
		} break;
		case ed::ShaderVariable::ValueType::Float1:
			ret |= ImGui::DragFloat(("##valuedit" + id).c_str(), (float*)data, 0.01f);
			break;
		case ed::ShaderVariable::ValueType::Float2:
			ret |= ImGui::DragFloat2(("##valuedit" + id).c_str(), (float*)data, 0.01f);
			break;
		case ed::ShaderVariable::ValueType::Float3:
			ret |= ImGui::DragFloat3(("##valuedit" + id).c_str(), (float*)data, 0.01f);
			break;
		case ed::ShaderVariable::ValueType::Float4:
			ret |= ImGui::DragFloat4(("##valuedit_" + id).c_str(), (float*)data, 0.01f);
			break;
		case ed::ShaderVariable::ValueType::Integer1:
			ret |= ImGui::DragInt(("##valuedit" + id).c_str(), (int*)data, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Integer2:
			ret |= ImGui::DragInt2(("##valuedit" + id).c_str(), (int*)data, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Integer3:
			ret |= ImGui::DragInt3(("##valuedit" + id).c_str(), (int*)data, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Integer4:
			ret |= ImGui::DragInt4(("##valuedit" + id).c_str(), (int*)data, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Boolean1:
			ret |= ImGui::DragScalar(("##valuedit" + id).c_str(), ImGuiDataType_U8, (int*)data, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Boolean2:
			ret |= ImGui::DragScalarN(("##valuedit" + id).c_str(), ImGuiDataType_U8, (int*)data, 2, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Boolean3:
			ret |= ImGui::DragScalarN(("##valuedit" + id).c_str(), ImGuiDataType_U8, (int*)data, 3, 1.0f);
			break;
		case ed::ShaderVariable::ValueType::Boolean4:
			ret |= ImGui::DragScalarN(("##valuedit" + id).c_str(), ImGuiDataType_U8, (int*)data, 4, 1.0f);
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
	void ObjectPreviewUI::Close(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (m_items[i].Name == name) {
				m_items.erase(m_items.begin() + i);
				i--;
			}
		}
	}
}