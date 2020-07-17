#pragma once
#include <SHADERed/UI/Tools/CubemapPreview.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class PixelInspectUI : public UIView {
	public:
		PixelInspectUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
			m_cubePrev.Init(152, 114);
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:
		std::string m_cacheExpression;
		std::string m_cacheValue;
		bool m_cacheHasColor;
		bool m_cacheExists;
		glm::vec4 m_cacheColor;

		CubemapPreview m_cubePrev;
	};
}