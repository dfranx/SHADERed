#pragma once
#include <SHADERed/Engine/Timer.h>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <SHADERed/Objects/ArcBallCamera.h>
#include <SHADERed/Objects/FirstPersonCamera.h>
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderVariable.h>
#include <glm/gtx/euler_angles.hpp>

#include <unordered_map>

namespace ed {
	// singleton used for getting some system-level values
	class SystemVariableManager {
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
			m_curState.WASD = glm::vec4(0, 0, 0, 0);
			m_curState.Viewport = glm::vec2(0, 1);
			m_curState.MousePosition = glm::vec2(0, 0);
			m_curState.DeltaTime = 0.0f;
			m_curState.IsSavingToFile = false;
			m_curGeoTransform.clear();
			m_prevGeoTransform.clear();
		}

		static inline ed::ShaderVariable::ValueType GetType(ed::SystemShaderVariable sysVar)
		{
			switch (sysVar) {
			case ed::SystemShaderVariable::MousePosition: return ed::ShaderVariable::ValueType::Float2;
			case ed::SystemShaderVariable::Mouse: return ed::ShaderVariable::ValueType::Float4;
			case ed::SystemShaderVariable::MouseButton: return ed::ShaderVariable::ValueType::Float4;
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
			case ed::SystemShaderVariable::IsSavingToFile: return ed::ShaderVariable::ValueType::Boolean1;
			case ed::SystemShaderVariable::CameraPosition: return ed::ShaderVariable::ValueType::Float4;
			case ed::SystemShaderVariable::CameraPosition3: return ed::ShaderVariable::ValueType::Float3;
			case ed::SystemShaderVariable::CameraDirection3: return ed::ShaderVariable::ValueType::Float3;
			case ed::SystemShaderVariable::KeysWASD: return ed::ShaderVariable::ValueType::Integer4;
			case ed::SystemShaderVariable::PickPosition: return ed::ShaderVariable::ValueType::Float3;
			}

			return ed::ShaderVariable::ValueType::Float1;
		}

		void Update(ed::ShaderVariable* var, void* item = nullptr);

		static ed::SystemShaderVariable GetTypeFromName(const std::string& name);

		void Reset();
		void CopyState();

		inline Camera* GetCamera() { return Settings::Instance().Project.FPCamera ? (Camera*)&m_curState.FPCam : (Camera*)&m_curState.ArcCam; }
		inline glm::mat4 GetViewMatrix() { return Settings::Instance().Project.FPCamera ? m_curState.FPCam.GetMatrix() : m_curState.ArcCam.GetMatrix(); }
		inline glm::mat4 GetProjectionMatrix() { return glm::perspective(glm::radians(45.0f), m_curState.Viewport.x / m_curState.Viewport.y, 0.1f, 1000.0f); }
		inline glm::mat4 GetOrthographicMatrix() { return glm::ortho(0.0f, m_curState.Viewport.x, m_curState.Viewport.y, 0.0f, 0.1f, 1000.0f); }
		inline glm::mat4 GetViewProjectionMatrix() { return GetProjectionMatrix() * GetViewMatrix(); }
		inline glm::mat4 GetViewOrthographicMatrix() { return GetOrthographicMatrix() * GetViewMatrix(); }
		inline const glm::mat4& GetGeometryTransform(PipelineItem* item) { return m_curGeoTransform[item]; }
		inline const glm::vec2& GetViewportSize() { return m_curState.Viewport; }
		inline const glm::ivec4& GetKeysWASD() { return m_curState.WASD; }
		inline const glm::vec2& GetMousePosition() { return m_curState.MousePosition; }
		inline const glm::vec4& GetMouse() { return m_curState.Mouse; }
		inline const glm::vec4& GetMouseButton() { return m_curState.MouseButton; }
		inline const glm::vec3& GetPickPosition() { return m_curState.PickPosition; }
		inline unsigned int GetFrameIndex() { return m_curState.FrameIndex; }
		inline float GetTime() { return m_timer.GetElapsedTime() + m_advTimer; }
		inline eng::Timer& GetTimeClock() { return m_timer; }
		inline float GetTimeDelta() { return m_curState.DeltaTime; }
		inline bool IsPicked() { return m_curState.IsPicked; }
		inline bool IsSavingToFile() { return m_curState.IsSavingToFile; }

		inline void SetGeometryTransform(PipelineItem* item, const glm::vec3& scale, const glm::vec3& rota, const glm::vec3& pos)
		{
			m_curGeoTransform[item] = glm::translate(glm::mat4(1), pos) * glm::yawPitchRoll(rota.y, rota.x, rota.z) * glm::scale(glm::mat4(1.0f), scale);
		}
		inline void SetViewportSize(float x, float y) { m_curState.Viewport = glm::vec2(x, y); }
		inline void SetMousePosition(float x, float y) { m_curState.MousePosition = glm::vec2(x, y); }
		inline void SetMouse(float x, float y, float left, float right) { m_curState.Mouse = glm::vec4(x, y, left, right); }
		inline void SetMouseButton(float x, float y, float left, float right) { m_curState.MouseButton = glm::vec4(x, y, left, right); }
		inline void SetTimeDelta(float x) { m_curState.DeltaTime = x; }
		inline void SetPicked(bool picked) { m_curState.IsPicked = picked; }
		inline void SetKeysWASD(int w, int a, int s, int d) { m_curState.WASD = glm::ivec4(w, a, s, d); }
		inline void SetFrameIndex(unsigned int ind) { m_curState.FrameIndex = ind; }
		inline void SetSavingToFile(bool isSaving) { m_curState.IsSavingToFile = isSaving; }
		inline void SetPickPosition(const glm::vec3& pos) { m_curState.PickPosition = pos; }

		inline void AdvanceTimer(float t) { m_advTimer += t; }

	private:
		eng::Timer m_timer;
		float m_advTimer;


		struct ValueGroup {
			float DeltaTime;
			ArcBallCamera ArcCam;
			FirstPersonCamera FPCam;
			glm::vec2 Viewport, MousePosition;
			bool IsPicked;
			bool IsSavingToFile;
			unsigned int FrameIndex;
			glm::ivec4 WASD;
			glm::vec4 Mouse, MouseButton;
			glm::vec3 PickPosition;
		} m_prevState, m_curState;

		std::unordered_map<PipelineItem*, glm::mat4> m_curGeoTransform, m_prevGeoTransform;
	};
}