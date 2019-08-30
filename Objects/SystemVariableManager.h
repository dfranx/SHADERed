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

		SystemVariableManager()
		{
			m_curState.FrameIndex = 0;
			m_curState.IsPicked = false;
			m_curState.WASD = glm::vec4(0,0,0,0);
			m_curState.Viewport = glm::vec2(0,0);
			m_curState.Mouse = glm::vec2(0,0);
			m_curState.DeltaTime = 0.0f;
			m_curState.GeometryTransform = glm::identity<glm::mat4>();
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
		void CopyState();

		inline Camera* GetCamera() { return Settings::Instance().Project.FPCamera ? (Camera*)&m_curState.FPCam : (Camera*)&m_curState.ArcCam; }
		inline glm::mat4 GetViewMatrix() { return Settings::Instance().Project.FPCamera ? m_curState.FPCam.GetMatrix() : m_curState.ArcCam.GetMatrix(); }
		inline glm::mat4 GetProjectionMatrix() { return glm::perspective(glm::radians(45.0f), m_curState.Viewport.x / m_curState.Viewport.y, 0.1f, 1000.0f); }
		inline glm::mat4 GetOrthographicMatrix() { return glm::ortho(0.0f, m_curState.Viewport.x, m_curState.Viewport.y, 0.0f, 0.1f, 1000.0f); }
		inline glm::mat4 GetViewProjectionMatrix() { return GetProjectionMatrix() * GetViewMatrix(); }
		inline glm::mat4 GetViewOrthographicMatrix() { return GetOrthographicMatrix() * GetViewMatrix(); }
		inline glm::mat4 GetGeometryTransform() { return m_curState.GeometryTransform; }
		inline glm::vec2 GetViewportSize() { return m_curState.Viewport; }
		inline glm::ivec4  GetKeysWASD() { return m_curState.WASD; }
		inline glm::vec2 GetMousePosition() { return m_curState.Mouse; }
		inline unsigned int GetFrameIndex() { return m_curState.FrameIndex; }
		inline float GetTime() { return m_timer.GetElapsedTime(); }
		inline float GetTimeDelta() { return m_curState.DeltaTime; }
		inline bool IsPicked() { return m_curState.IsPicked; }

		inline void SetGeometryTransform(const glm::vec3& scale, const glm::vec3& rota, const glm::vec3& pos)
		{
			m_curState.GeometryTransform = glm::translate(glm::mat4(1), pos) *
				glm::yawPitchRoll(rota.y, rota.x, rota.z) * 
				glm::scale(glm::mat4(1.0f), scale);
		}
		inline void SetViewportSize(float x, float y) { m_curState.Viewport = glm::vec2(x, y); }
		inline void SetMousePosition(float x, float y) { m_curState.Mouse = glm::vec2(x, y); }
		inline void SetTimeDelta(float x) { m_curState.DeltaTime = x; }
		inline void SetPicked(bool picked) { m_curState.IsPicked = picked; }
		inline void SetKeysWASD(int w, int a, int s, int d) { m_curState.WASD = glm::ivec4(w, a, s, d); }
		inline void SetFrameIndex(unsigned int ind) { m_curState.FrameIndex = ind; }

	private:
		eng::Timer m_timer;

		struct ValueGroup
		{
			float DeltaTime;
			ArcBallCamera ArcCam;
			FirstPersonCamera FPCam;
			glm::vec2 Viewport, Mouse;
			glm::mat4 GeometryTransform;
			bool IsPicked;
			unsigned int FrameIndex;

			glm::ivec4 WASD;
		} m_prevState, m_curState;
	};
}