#pragma once
#include <glm/glm.hpp>

namespace ed {
	class Camera {
	public:
		virtual void Reset() {};

		virtual glm::vec3 GetRotation() = 0;

		virtual glm::vec4 GetPosition() = 0;
		virtual glm::vec4 GetUpVector() = 0;
		virtual glm::vec4 GetViewDirection() = 0;

		virtual glm::mat4 GetMatrix() = 0;
	};
}