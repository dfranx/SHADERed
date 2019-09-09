#include "ObjectPreviewUI.h"
#include <imgui/imgui.h>

namespace ed
{
    void ObjectPreviewUI::Open(const std::string& name, float w, float h, unsigned int item, bool isCube, void* rt, void* audio, void* model)
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
        i.Model = model;

        m_items.push_back(i);
    }
	void ObjectPreviewUI::OnEvent(const SDL_Event& e)
    {

    }
	void ObjectPreviewUI::Update(float delta)
    {
        for (int i = 0; i < m_items.size(); i++) {
            mItem* item = &m_items[i];

            if (!item->IsOpen)
                continue;

            std::string& name = item->Name;

            if (ImGui::Begin((name + "###objprev" + std::to_string(i)).c_str(), &item->IsOpen)) {
                ImVec2 aSize = ImGui::GetContentRegionAvail();
                
                glm::ivec2 iSize(item->Width, item->Height);
                if (item->RT != nullptr)
                    iSize = m_data->Objects.GetRenderTextureSize(name);

                float scale = std::min(aSize.x/iSize.x, aSize.y/iSize.y);
                aSize.x = iSize.x * scale;
                aSize.y = iSize.y * scale;

                if (item->IsCube) {
                    ImVec2 posSize = ImGui::GetContentRegionAvail();
                    float posX = (posSize.x - aSize.x) / 2;
                    float posY = (posSize.y - aSize.y) / 2;
                    ImGui::SetCursorPosX(posX);
                    ImGui::SetCursorPosY(posY);
                    
                    m_cubePrev.Draw(item->Texture);
                    ImGui::Image((void*)(intptr_t)m_cubePrev.GetTexture(), aSize, ImVec2(0, 1), ImVec2(1, 0));
                } else if (item->Audio != nullptr) {
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
                        m_samples[i] = sf* 0.5f + 0.5f;
                    }
                    
                    ImGui::PlotHistogram("Frequencies", m_fft, IM_ARRAYSIZE(m_fft), 0, NULL, 0.0f, 1.0f, ImVec2(0,80));
                    ImGui::PlotHistogram("Samples", m_samples, IM_ARRAYSIZE(m_samples), 0, NULL, 0.0f, 1.0f, ImVec2(0,80));
                } else {
                    ImVec2 posSize = ImGui::GetContentRegionAvail();
                    float posX = (posSize.x - aSize.x) / 2;
                    float posY = (posSize.y - aSize.y) / 2;
                    ImGui::SetCursorPosX(posX);
                    ImGui::SetCursorPosY(posY);
                
                    ImGui::Image((void*)(intptr_t)item->Texture, aSize, ImVec2(0, 1), ImVec2(1, 0));
                }
            }
            ImGui::End();

            if (!item->IsOpen) {
                m_items.erase(m_items.begin() + i);
                i--;
            }
        }
    }
    void ObjectPreviewUI::Close(const std::string& name)
    {
        for (int i = 0; i < m_items.size(); i++) {
            if (m_items[i].Name == name) {
                m_items.erase(m_items.begin() + 1);
                i--;
            }
        }
    }
}