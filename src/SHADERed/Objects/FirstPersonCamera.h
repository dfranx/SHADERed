#pragma once
#include <SHADERed/Objects/Camera.h>
#include <glm/glm.hpp>

namespace ed {
	class FirstPersonCamera : public Camera {
	public:
		FirstPersonCamera()
				: m_yaw(0.0f)
				, m_pitch(0.0f)
		{
			Reset();
		}

		virtual void Reset();

		inline void SetPosition(float x, float y, float z) { m_pos = glm::vec3(x, y, z); }

		void MoveLeftRight(float d);
		void MoveUpDown(float d);
		inline void Yaw(float y) { m_yaw -= y; }
		inline void Pitch(float p) { m_pitch -= p; }
		inline void SetYaw(float y) { m_yaw = y; }
		inline void SetPitch(float p) { m_pitch = p; }

		virtual inline glm::vec3 GetRotation() { return glm::vec3(m_yaw, m_pitch, 0); }

		virtual inline glm::vec4 GetPosition() { return glm::vec4(m_pos, 0.0f); }
		virtual glm::vec4 GetUpVector();
		virtual glm::vec4 GetViewDirection();

		virtual glm::mat4 GetMatrix();

		inline FirstPersonCamera& operator=(const FirstPersonCamera& fp)
		{
			this->m_pos = fp.m_pos;
			this->m_yaw = fp.m_yaw;
			this->m_pitch = fp.m_pitch;
			return *this;
		}

	private:
		glm::vec3 m_pos;

		float m_yaw;
		float m_pitch;
	};
}
