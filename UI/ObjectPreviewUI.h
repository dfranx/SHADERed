#pragma once
#include "UIView.h"
#include "../Objects/PipelineItem.h"

namespace ed
{
	class ObjectPreviewUI : public UIView
	{
	public:
		ObjectPreviewUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true) :
			UIView(ui, objects, name, visible)
		{ }
		~ObjectPreviewUI() { }

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

        void Open(const std::string& name, float w, float h, unsigned int item, bool isCube = false, void* rt = nullptr, void* audio = nullptr);

        inline bool ShouldRun() { return m_items.size() > 0; }
        inline void CloseAll() { m_items.clear(); }

	protected:
        struct mItem
        {
            std::string Name;
            float Width, Height;
            unsigned int Texture;
            bool IsCube;
            bool IsOpen;

            void* RT;
            void* Audio; // TODO this
        };

    private:
        std::vector<mItem> m_items;
	};
}