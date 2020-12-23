#pragma once
#include <SHADERed/UI/UIView.h>
#include <SHADERed/Objects/ArcBallCamera.h>
#include <SHADERed/Engine/Model.h>

namespace ed {
	class DebugVectorWatchUI : public UIView {
	public:
		DebugVectorWatchUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
			m_newExpr[0] = 0;
			m_tempExpr[0] = 0;
			m_init();
		}
		~DebugVectorWatchUI();

		void Refresh();

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		inline ArcBallCamera* GetCamera() { return &m_camera; }

	private:
		char m_tempExpr[512];
		char m_newExpr[512];

		eng::Model m_vectorHandle, m_vectorPoint;
		float m_vectorPointSize;

		float m_lastFBOWidth, m_lastFBOHeight;

		ArcBallCamera m_camera;
		unsigned int m_unitSphereVAO, m_unitSphereVBO;

		unsigned int m_xVAO, m_xVBO;
		unsigned int m_yVAO, m_yVBO;
		unsigned int m_zVAO, m_zVBO;

		unsigned int m_gridVAO, m_gridVBO;

		unsigned int m_uMatVP, m_uMatWorld, m_uColor;

		unsigned int m_fbo, m_fboColor, m_fboDepth;
		unsigned int m_simpleShader;
		void m_init();

		ImVec2 m_mouseContact;

		void m_render();
	};
}