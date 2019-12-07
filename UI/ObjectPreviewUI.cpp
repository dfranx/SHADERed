#include "ObjectPreviewUI.h"
#include "../Objects/Names.h"
#include "../Objects/SystemVariableManager.h"
#include <imgui/imgui.h>

namespace ed
{
    void ObjectPreviewUI::Open(const std::string& name, float w, float h, unsigned int item, bool isCube, void* rt, void* audio, void* buffer, void* plugin)
    {
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
        m_zoom.push_back(Magnifier()); // TODO: only create magnifier tools for textures to lower down GPU resource usage
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
            else if (e.type == SDL_MOUSEBUTTONUP)
                m_zoom[m_curHoveredItem].EndMouseAction();
        }
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
					pobj->Owner->ShowObjectExtendedPreview(pobj->Type, pobj->Data, pobj->ID);
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

						m_cubePrev.Draw(item->Texture);
						const glm::vec2& zPos = m_zoom[i].GetZoomPosition();
						const glm::vec2& zSize = m_zoom[i].GetZoomSize();
						ImGui::Image((void*)(intptr_t)m_cubePrev.GetTexture(), aSize, ImVec2(zPos.x, zPos.y + zSize.y), ImVec2(zPos.x + zSize.x, zPos.y));

						if (ImGui::IsItemHovered()) m_curHoveredItem = i;
						if (m_zoom[i].IsSelecting() && m_lastZoomSize != glm::vec2(aSize.x, aSize.y)) {
							m_lastZoomSize = glm::vec2(aSize.x, aSize.y);
							m_zoom[i].RebuildVBO(aSize.x, aSize.y);
						}
						if (m_curHoveredItem == i && ImGui::GetIO().KeyAlt && ImGui::IsMouseDoubleClicked(0))
							m_zoom[i].Reset();
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
							perRow += ShaderVariable::GetSize(item->CachedFormat[i]);
						ImGui::Text("Size per row: %d bytes", perRow);

						ImGui::Text("Size in bytes:");
						ImGui::SameLine();
						ImGui::PushItemWidth(200);
						ImGui::InputInt("##objprev_bsize", &item->CachedSize, 1, 100, ImGuiInputTextFlags_AlwaysInsertMode);
						ImGui::PopItemWidth();
						ImGui::SameLine();
						if (ImGui::Button("APPLY##objprev_applysize")) {
							buf->Size = item->CachedSize;
							if (buf->Size < 0) buf->Size = 0;

							buf->Data = realloc(buf->Data, buf->Size);

							glBindBuffer(GL_UNIFORM_BUFFER, buf->ID);
							glBufferData(GL_UNIFORM_BUFFER, buf->Size, buf->Data, GL_STATIC_DRAW); // resize
							glBindBuffer(GL_UNIFORM_BUFFER, 0);

							m_data->Parser.ModifyProject();
						}
						if (ImGui::Button("CLEAR##objprev_clearbuf")) {
							memset(buf->Data, 0, buf->Size);

							glBindBuffer(GL_UNIFORM_BUFFER, buf->ID);
							glBufferData(GL_UNIFORM_BUFFER, buf->Size, buf->Data, GL_STATIC_DRAW); // resize
							glBindBuffer(GL_UNIFORM_BUFFER, 0);

							m_data->Parser.ModifyProject();
						}

						// update buffer data every 330ms
						ImGui::Text("Buffer view is updated every 330ms");
						if (m_bufUpdateClock.getElapsedTime().asMilliseconds() > 330) {
							glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf->ID);
							glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, buf->Size, buf->Data);
							glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
							m_bufUpdateClock.restart();
						}

						if (perRow != 0) {
							ImGui::Separator();

							int rows = buf->Size / perRow;
							ImGui::Columns(item->CachedFormat.size());

							for (int j = 0; j < item->CachedFormat.size(); j++) {
								ImGui::Text(VARIABLE_TYPE_NAMES[(int)item->CachedFormat[j]]);
								ImGui::NextColumn();
							}

							for (int i = 0; i < rows; i++) {
								int curColOffset = 0;
								for (int j = 0; j < item->CachedFormat.size(); j++) {
									int dOffset = i * perRow + curColOffset;
									if (m_drawBufferElement(i, j, (void*)(((char*)buf->Data) + dOffset), item->CachedFormat[j])) {
										glBindBuffer(GL_UNIFORM_BUFFER, buf->ID);
										glBufferData(GL_UNIFORM_BUFFER, buf->Size, buf->Data, GL_STATIC_DRAW); // allocate 0 bytes of memory
										glBindBuffer(GL_UNIFORM_BUFFER, 0);

										m_data->Parser.ModifyProject();
									}
									curColOffset += ShaderVariable::GetSize(item->CachedFormat[j]);
									ImGui::NextColumn();
								}
							}

							ImGui::Columns(1);
						}

					}
					else {
						ImVec2 posSize = ImGui::GetContentRegionAvail();
						float posX = (posSize.x - aSize.x) / 2;
						float posY = (posSize.y - aSize.y) / 2;
						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY);

						const glm::vec2& zPos = m_zoom[i].GetZoomPosition();
						const glm::vec2& zSize = m_zoom[i].GetZoomSize();
						ImGui::Image((void*)(intptr_t)item->Texture, aSize, ImVec2(zPos.x, zPos.y + zSize.y), ImVec2(zPos.x + zSize.x, zPos.y));

						if (ImGui::IsItemHovered()) m_curHoveredItem = i;
						if (m_zoom[i].IsSelecting() && m_lastZoomSize != glm::vec2(aSize.x, aSize.y)) {
							m_lastZoomSize = glm::vec2(aSize.x, aSize.y);
							m_zoom[i].RebuildVBO(aSize.x, aSize.y);
						}
						if (m_curHoveredItem == i && ImGui::GetIO().KeyAlt && ImGui::IsMouseDoubleClicked(0))
							m_zoom[i].Reset();
					}
				}
			}
            ImGui::End();

            if (!item->IsOpen) {
                m_items.erase(m_items.begin() + i);
                i--;
            }
        }
    }
    bool ObjectPreviewUI::m_drawBufferElement(int row, int col, void *data, ShaderVariable::ValueType type)
    {
        bool ret = false;

		ImGui::PushItemWidth(-1);

        std::string id = std::to_string(row) + std::to_string(col);

		switch (type) {
			case ed::ShaderVariable::ValueType::Float4x4:
			case ed::ShaderVariable::ValueType::Float3x3:
			case ed::ShaderVariable::ValueType::Float2x2:
            {
                int cols = 2;
                if (type == ShaderVariable::ValueType::Float3x3)
                    cols = 3;
                else if (type == ShaderVariable::ValueType::Float4x4)
                    cols = 4;

				for (int y = 0; y < cols; y++) {
					if (type == ShaderVariable::ValueType::Float2x2)
						ret = ret || ImGui::DragFloat2(("##valuedit" + id + std::to_string(y)).c_str(), (float*)data, 0.01f);
					else if (type == ShaderVariable::ValueType::Float3x3)
						ret = ret || ImGui::DragFloat3(("##valuedit" + id + std::to_string(y)).c_str(), (float*)data, 0.01f);
					else ret = ret || ImGui::DragFloat4(("##valuedit" + id + std::to_string(y)).c_str(), (float*)data, 0.01f);
				}
            } break;
			case ed::ShaderVariable::ValueType::Float1:
				ret = ret || ImGui::DragFloat(("##valuedit" + id).c_str(), (float*)data, 0.01f);
				break;
			case ed::ShaderVariable::ValueType::Float2:
				ret = ret || ImGui::DragFloat2(("##valuedit" + id).c_str(), (float*)data, 0.01f);
				break;
			case ed::ShaderVariable::ValueType::Float3:
				ret = ret || ImGui::DragFloat3(("##valuedit" + id).c_str(), (float*)data, 0.01f);
				break;
			case ed::ShaderVariable::ValueType::Float4:
				ret = ret || ImGui::DragFloat4(("##valuedit_" + id).c_str(), (float*)data, 0.01f);
				break;
			case ed::ShaderVariable::ValueType::Integer1:
				ret = ret || ImGui::DragInt(("##valuedit" + id).c_str(), (int*)data, 0.3f);
				break;
			case ed::ShaderVariable::ValueType::Integer2:
				ret = ret || ImGui::DragInt2(("##valuedit" + id).c_str(), (int*)data, 0.3f);
				break;
			case ed::ShaderVariable::ValueType::Integer3:
				ret = ret || ImGui::DragInt3(("##valuedit" + id).c_str(), (int*)data, 0.3f);
				break;
			case ed::ShaderVariable::ValueType::Integer4:
				ret = ret || ImGui::DragInt4(("##valuedit" + id).c_str(), (int*)data, 0.3f);
				break;
			case ed::ShaderVariable::ValueType::Boolean1:
				ret = ret || ImGui::Checkbox(("##valuedit" + id).c_str(), (bool*)data);
				break;
			case ed::ShaderVariable::ValueType::Boolean2:
				ret = ret || ImGui::Checkbox(("##valuedit1" + id).c_str(), (bool*)(data)+0); ImGui::SameLine();
				ret = ret || ImGui::Checkbox(("##valuedit2" + id).c_str(), (bool*)(data)+1);
				break;
			case ed::ShaderVariable::ValueType::Boolean3:
				ret = ret || ImGui::Checkbox(("##valuedit1" + id).c_str(), (bool*)(data)+0); ImGui::SameLine();
				ret = ret || ImGui::Checkbox(("##valuedit2" + id).c_str(), (bool*)(data)+1); ImGui::SameLine();
				ret = ret || ImGui::Checkbox(("##valuedit3" + id).c_str(), (bool*)(data)+2);
				break;
			case ed::ShaderVariable::ValueType::Boolean4:
				ret = ret || ImGui::Checkbox(("##valuedit1" + id).c_str(), (bool*)(data)+0); ImGui::SameLine();
				ret = ret || ImGui::Checkbox(("##valuedit2" + id).c_str(), (bool*)(data)+1); ImGui::SameLine();
				ret = ret || ImGui::Checkbox(("##valuedit3" + id).c_str(), (bool*)(data)+2); ImGui::SameLine();
				ret = ret || ImGui::Checkbox(("##valuedit4" + id).c_str(), (bool*)(data)+3);
				break;
		}

		ImGui::PopItemWidth();

        return ret;
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