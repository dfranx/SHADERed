#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	class PropertyUI : public UIView {
	public:
		PropertyUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
			m_init();
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		void Open(PipelineItem* item);
		void Open(ObjectManagerItem* obj);
		inline void Close() {
			m_current = nullptr;
			m_currentObj = nullptr;
		}
		inline std::string CurrentItemName() { return m_current != nullptr ? m_current->Name : (m_currentObj != nullptr ? m_currentObj->Name : ""); }
		inline bool HasItemSelected() { return m_current != nullptr || m_currentObj != nullptr; }

		inline bool IsPipelineItem() { return m_current != nullptr; }
		inline bool IsRenderTexture() { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::RenderTexture; }
		inline bool IsTexture() { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::Texture; }
		inline bool IsImage() { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::Image; }
		inline bool IsImage3D() { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::Image3D; }
		inline bool IsTexture3D() { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::Texture3D; }
		inline bool IsPlugin() { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::PluginObject; }

	private:
		char m_itemName[2048];

		void m_init();

		PipelineItem* m_current;
		ObjectManagerItem* m_currentObj;

		char* m_dialogPath;
		std::string m_dialogShaderType;

		glm::ivec3 m_cachedGroupSize;
	};
}