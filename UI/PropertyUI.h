#pragma once
#include "UIView.h"

namespace ed
{
	class PropertyUI : public UIView
	{
	public:
		PropertyUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true) : UIView(ui, objects, name, visible)
		{
			m_init();
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		void Open(PipelineItem *item);
		void Open(const std::string &name, RenderTextureObject *obj);
		void Open(const std::string& name, ImageObject* obj);
		void Open(const std::string& name, Image3DObject* obj);
		inline std::string CurrentItemName() { return m_current != nullptr ? m_current->Name : (m_currentRT != nullptr ? m_currentRT->Name : (m_currentImg3D != nullptr ? m_data->Objects.GetImage3DNameByID(m_currentImg3D->Texture) : m_data->Objects.GetImageNameByID(m_currentImg->Texture))); }
		inline bool HasItemSelected() { return m_current != nullptr || m_currentRT != nullptr || m_currentImg != nullptr || m_currentImg3D != nullptr; }

		inline bool IsPipelineItem() { return m_current != nullptr; }
		inline bool IsRenderTexture() { return m_currentRT != nullptr; }
		inline bool IsImage() { return m_currentImg != nullptr; }
		inline bool IsImage3D() { return m_currentImg3D != nullptr; }

	private:
		char m_itemName[64];

		void m_init();

		PipelineItem* m_current;
		RenderTextureObject* m_currentRT;
		ImageObject* m_currentImg;
		Image3DObject* m_currentImg3D;

		glm::ivec3 m_cachedGroupSize;
	};
}