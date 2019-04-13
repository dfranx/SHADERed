#pragma once
#include "UIView.h"
#include "../ImGUI/imgui.h"
#include "../Objects/GizmoObject.h"

namespace ed
{
	class PreviewUI : public UIView
	{
	public:
		PreviewUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true) :
			UIView(ui, objects, name, visible),
			m_pick(nullptr),
			m_pickMode(0),
			m_gizmo(objects->GetOwner()),
			m_lastSize(-1, -1) {
			m_setupShortcuts();
			m_fpsLimit = m_elapsedTime = 0;
		}

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

		inline void Pick(PipelineItem* item) { m_pick = item; }

	private:
		void m_setupShortcuts();

		ImVec2 m_mouseContact;
		GizmoObject m_gizmo;

		DirectX::XMFLOAT3 m_pos1, m_pos2;

		ml::Timer m_fpsTimer;
		float m_fpsDelta;
		float m_fpsUpdateTime; // check if 0.5s passed then update the fps widget

		ml::RenderTexture m_rt;
		ml::ShaderResourceView m_rtView;
		DirectX::XMINT2 m_lastSize;

		float m_elapsedTime;
		float m_fpsLimit;

		PipelineItem* m_pick;
		int m_pickMode; // 0 = position, 1 = scale, 2 = rotation
	};
}