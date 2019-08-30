#pragma once
#include "../Engine/Timer.h"
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include "ShaderVariable.h"
#include "ArcBallCamera.h"
#include "FirstPersonCamera.h"
#include "Settings.h"

namespace ed
{
	// singleton used for getting some system-level values
	class SystemVariableManager
	{
	public:
		static inline SystemVariableManager& Instance()
		{
			static SystemVariableManager ret;
			return ret;
		}

		SystemVariableManager() : 
			m_frameIndex(0),
			m_deltaTime(0.0f),
			m_viewport(0,1), m_mouse(0,0),
			m_isPicked(false),
			m_wasd(0,0,0,0) {
			m_geometryTransform = glm::identity<glm::mat4>();
		}

		static inline ed::ShaderVariable::ValueType GetType(ed::SystemShaderVariable sysVar)
		{
			switch (sysVar) {
				case ed::SystemShaderVariable::MousePosition: return ed::ShaderVariable::ValueType::Float2;
				case ed::SystemShaderVariable::Projection: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::Time: return ed::ShaderVariable::ValueType::Float1;
				case ed::SystemShaderVariable::TimeDelta: return ed::ShaderVariable::ValueType::Float1;
				case ed::SystemShaderVariable::FrameIndex: return ed::ShaderVariable::ValueType::Integer1;
				case ed::SystemShaderVariable::View: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::ViewportSize: return ed::ShaderVariable::ValueType::Float2;
				case ed::SystemShaderVariable::ViewProjection: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::Orthographic: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::ViewOrthographic: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::GeometryTransform: return ed::ShaderVariable::ValueType::Float4x4;
				case ed::SystemShaderVariable::IsPicked: return ed::ShaderVariable::ValueType::Boolean1;
				case ed::SystemShaderVariable::CameraPosition: return ed::ShaderVariable::ValueType::Float4;
				case ed::SystemShaderVariable::KeysWASD: return ed::ShaderVariable::ValueType::Integer4;
			}

			return ed::ShaderVariable::ValueType::Float1;
		}

		void Update(ed::ShaderVariable* var);

		void Reset();

		inline Camera* GetCamera() { return Settings::Instance().Project.FPCamera ? (Camera*)&m_fpCam : (Camera*)&m_abCam; }
		inline glm::mat4 GetViewMatrix() { return Settings::Instance().Project.FPCamera ? m_fpCam.GetMatrix() : m_abCam.GetMatrix(); }
		inline glm::mat4 GetProjectionMatrix() { return glm::perspective(glm::radians(45.0f), m_viewport.x / m_viewport.y, 0.1f, 1000.0f); }
		inline glm::mat4 GetOrthographicMatrix() { return glm::ortho(0.0f, m_viewport.x, m_viewport.y, 0.0f, 0.1f, 1000.0f); }
		inline glm::mat4 GetViewProjectionMatrix() { return GetProjectionMatrix() * GetViewMatrix(); }
		inline glm::mat4 GetViewOrthographicMatrix() { return GetOrthographicMatrix() * GetViewMatrix(); }
		inline glm::mat4 GetGeometryTransform() { return m_geometryTransform; }
		inline glm::vec2 GetViewportSize() { return m_viewport; }
		inline glm::ivec4  GetKeysWASD() { return m_wasd; }
		inline glm::vec2 GetMousePosition() { return m_mouse; }
		inline unsigned int GetFrameIndex() { return m_frameIndex; }
		inline float GetTime() { return m_timer.GetElapsedTime(); }
		inline float GetTimeDelta() { return m_deltaTime; }
		inline bool IsPicked() { return m_isPicked; }

		inline void SetGeometryTransform(const glm::vec3& scale, const glm::vec3& rota, const glm::vec3& pos)
		{
			m_geometryTransform = glm::translate(glm::mat4(1), pos) *
				glm::yawPitchRoll(rota.y, rota.x, rota.z) * 
				glm::scale(glm::mat4(1.0f), scale);
		}
		inline void SetViewportSize(float x, float y) { m_viewport = glm::vec2(x, y); }
		inline void SetMousePosition(float x, float y) { m_mouse = glm::vec2(x, y); }
		inline void SetTimeDelta(float x) { m_deltaTime = x; }
		inline void SetPicked(bool picked) { m_isPicked = picked; }
		inline void SetKeysWASD(int w, int a, int s, int d) { m_wasd = glm::ivec4(w, a, s, d); }
		inline void SetFrameIndex(unsigned int ind) { m_frameIndex = ind; }

	private:
		eng::Timer m_timer;
		float m_deltaTime;
		ArcBallCamera m_abCam;
		FirstPersonCamera m_fpCam;
		glm::vec2 m_viewport, m_mouse;
		glm::mat4 m_geometryTransform;
		bool m_isPicked;
		unsigned int m_frameIndex;

		glm::ivec4 m_wasd;
	};
}