#include "ObjectPreviewUI.h"
#include <imgui/imgui.h>

namespace ed
{
    void ObjectPreviewUI::Open(const std::string& name, float w, float h, unsigned int item, bool isCube, void* rt, void* audio)
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
                ImVec2 posSize = aSize;

                glm::ivec2 iSize(item->Width, item->Height);
                if (item->RT != nullptr)
                    iSize = m_data->Objects.GetRenderTextureSize(name);

                float scale = std::min(aSize.x/iSize.x, aSize.y/iSize.y);
                aSize.x = iSize.x * scale;
                aSize.y = iSize.y * scale;

                float posX = (posSize.x - aSize.x) / 2;
                float posY = (posSize.y - aSize.y) / 2;
                ImGui::SetCursorPosX(posX);
                ImGui::SetCursorPosY(posY);
                
                //if (item->Width > aSize.x || item->Height > aSize.y) {
                //    float height 
                //}

                ImGui::Image((void*)(intptr_t)item->Texture, aSize, ImVec2(0, 1), ImVec2(1, 0));
            }
            ImGui::End();

            if (!item->IsOpen) {
                m_items.erase(m_items.begin() + i);
                i--;
            }
        }
    }
}