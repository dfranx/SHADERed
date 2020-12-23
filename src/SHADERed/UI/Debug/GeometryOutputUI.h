#pragma once
#include <SHADERed/UI/UIView.h>
#include <SHADERed/Objects/ArcBallCamera.h>

namespace ed {
	class DebugGeometryOutputUI : public UIView {
	public:
		DebugGeometryOutputUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true);
		~DebugGeometryOutputUI();

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		inline ArcBallCamera* GetCamera() { return &m_camera; }

	private:
		void m_buildVertexBuffers();
		void m_render();

		GLuint m_bufPointsVAO, m_bufLinesVAO, m_bufTrianglesVAO;
		GLuint m_bufPointsVBO, m_bufLinesVBO, m_bufTrianglesVBO;
		GLuint m_fbo, m_fboColor, m_fboDepth;
		GLuint m_uMatLoc;

		int m_lines, m_points, m_triangles;
		
		int m_is3D;

		GLuint m_shader;

		ArcBallCamera m_camera;
		ImVec2 m_mouseContact;

		std::vector<ImVec2> m_indexPos;

		float m_lastFBOWidth, m_lastFBOHeight;
	};
}