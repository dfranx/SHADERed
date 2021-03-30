#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	class DebugTessControlOutputUI : public UIView {
	public:
		DebugTessControlOutputUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true);
		~DebugTessControlOutputUI();

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		void Refresh();

	private:
		void m_render();

		void m_buildShaders();

		glm::vec2 m_inner;
		glm::vec4 m_outer;
		bool m_error; // is any of the levels below 1.0?

		GLuint m_fbo, m_color, m_depth;
		ImVec2 m_lastSize;

		GLuint m_triangleVAO, m_triangleVBO;
		GLuint m_shader;
	};
}