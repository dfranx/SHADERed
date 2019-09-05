#pragma once
#include "UIView.h"

namespace ed
{
	class ObjectListUI : public UIView
	{
	public:
		ObjectListUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true) :
			UIView(ui, objects, name, visible)
		{
			m_setupCubemapPreview();
		}
		~ObjectListUI() {
			glDeleteBuffers(1, &m_fsVBO);
			glDeleteVertexArrays(1, &m_fsVAO);
			glDeleteTextures(1, &m_cubeTex);
			glDeleteTextures(1, &m_cubeDepth);
			glDeleteFramebuffers(1, &m_cubeFBO);
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:
		void m_setupCubemapPreview();
		void m_renderCubemap(GLuint tex);

		GLuint m_cubeShader;
		GLuint m_cubeFBO, m_cubeTex, m_cubeDepth;
		GLuint m_fsVAO, m_fsVBO;
		GLuint m_uMatWVPLoc;
	};
}