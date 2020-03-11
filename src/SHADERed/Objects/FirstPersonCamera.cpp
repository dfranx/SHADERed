#include <SHADERed/Objects/FirstPersonCamera.h>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <stdio.h>
#include <algorithm>
#include <glm/gtx/euler_angles.hpp>

#define FORWARD_VECTOR glm::vec4(0, 0, 1, 0)
#define RIGHT_VECTOR glm::vec4(1, 0, 0, 0)
#define UP_VECTOR glm::vec4(0, 1, 0, 0)

namespace ed {
	void FirstPersonCamera::Reset()
	{
		m_pos = glm::vec3(0.0f, 0.0f, 7.0f);
	}
	void FirstPersonCamera::MoveLeftRight(float d)
	{
		glm::mat4 rotaMatrix = glm::yawPitchRoll(glm::radians(m_yaw), glm::radians(m_pitch), 0.0f);
		glm::vec4 right = rotaMatrix * RIGHT_VECTOR;

		m_pos = glm::vec4(m_pos, 0) + right * d;
	}
	void FirstPersonCamera::MoveUpDown(float d)
	{
		glm::mat4 rotaMatrix = glm::yawPitchRoll(glm::radians(m_yaw), glm::radians(m_pitch), 0.0f);
		glm::vec4 forward = rotaMatrix * FORWARD_VECTOR;

		m_pos = glm::vec4(m_pos, 0) + forward * d;
	}
	glm::vec4 FirstPersonCamera::GetUpVector()
	{
		glm::mat4 yawMatrix = glm::rotate(glm::mat4(1), m_yaw, glm::vec3(0, 1, 0));

		return yawMatrix * glm::vec4(0, 1, 0, 1);
	}
	glm::mat4 FirstPersonCamera::GetMatrix()
	{
		glm::mat4 rota = glm::yawPitchRoll(glm::radians(m_yaw), glm::radians(m_pitch), 0.0f);

		glm::vec4 target = rota * FORWARD_VECTOR;
		target = glm::vec4(m_pos, 0) - glm::normalize(target);

		glm::mat4 yawMatrix = glm::rotate(glm::mat4(1), m_yaw, glm::vec3(0, 1, 0));

		glm::vec4 right = yawMatrix * RIGHT_VECTOR;
		glm::vec4 up = yawMatrix * UP_VECTOR;
		glm::vec4 forward = yawMatrix * FORWARD_VECTOR;

		return glm::lookAt(m_pos, glm::vec3(target), glm::vec3(up));
	}
	glm::vec4 FirstPersonCamera::GetViewDirection()
	{
		glm::mat4 rota = glm::yawPitchRoll(glm::radians(m_yaw), glm::radians(m_pitch), 0.0f);

		glm::vec4 target = rota * FORWARD_VECTOR;
		target = glm::vec4(m_pos, 0) - glm::normalize(target);

		return target;
	}
}
