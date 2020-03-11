#include <SHADERed/Objects/ArcBallCamera.h>
#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

namespace ed {
	const float ArcBallCamera::MaxDistance = 50.0f;
	const float ArcBallCamera::MinDistance = 2.0f;
	const float ArcBallCamera::MaxRotationY = 89.0f;

	void ArcBallCamera::Reset()
	{
		m_distance = 7;
		m_pitch = 0;
		m_yaw = 0;
		m_roll = 0;
	}
	void ArcBallCamera::SetDistance(float d)
	{
		m_distance = glm::clamp(d, ArcBallCamera::MinDistance, ArcBallCamera::MaxDistance);
	}
	void ArcBallCamera::Move(float d)
	{
		m_distance = glm::clamp(m_distance + d, ArcBallCamera::MinDistance, ArcBallCamera::MaxDistance);
	}
	void ArcBallCamera::Yaw(float rx)
	{
		m_yaw = fmod(m_yaw - rx, 360.0f);
		if (m_yaw < 0.0)
			m_yaw += 360.0f;
	}
	void ArcBallCamera::Pitch(float ry)
	{
		m_pitch = glm::clamp(m_pitch + ry, -ArcBallCamera::MaxRotationY, ArcBallCamera::MaxRotationY);
	}
	void ArcBallCamera::Roll(float rz)
	{
		m_roll = fmod(m_roll + rz, 360.0f);
		if (m_roll < 0.0)
			m_roll += 360.0f;
	}
	void ArcBallCamera::SetYaw(float r)
	{
		m_yaw = r;
	}
	void ArcBallCamera::SetPitch(float r)
	{
		m_pitch = r;
	}
	void ArcBallCamera::SetRoll(float r)
	{
		m_roll = r;
	}
	glm::vec4 ArcBallCamera::GetViewDirection()
	{
		return -GetPosition();
	}
	glm::vec4 ArcBallCamera::GetPosition()
	{
		glm::vec4 pos(0, 0, -m_distance, 0);
		glm::mat4 rotaMat = glm::yawPitchRoll(glm::radians(m_yaw), glm::radians(m_pitch), glm::radians(m_roll));

		pos = rotaMat * pos;

		return pos;
	}
	glm::vec4 ArcBallCamera::GetUpVector()
	{
		glm::vec4 up(0, 1, 0, 0);
		glm::mat4 rotaMat = glm::yawPitchRoll(glm::radians(m_yaw), glm::radians(m_pitch), glm::radians(m_roll));

		up = rotaMat * up;

		return glm::normalize(up);
	}
	glm::mat4 ArcBallCamera::GetMatrix()
	{
		glm::vec4 pos(0, 0, -m_distance, 0);
		glm::vec4 up(0, 1, 0, 0);
		glm::mat4 rotaMat = glm::yawPitchRoll(glm::radians(m_yaw), glm::radians(m_pitch), glm::radians(m_roll));

		up = rotaMat * up;
		pos = rotaMat * pos;

		return glm::lookAt(glm::vec3(pos), glm::vec3(0, 0, 0), glm::vec3(up));
	}
}