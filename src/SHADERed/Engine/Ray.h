#pragma once
#include <glm/glm.hpp>

namespace ed {
	namespace ray {
		bool IntersectBox(glm::vec3 orig, glm::vec3 dir, glm::vec3 minp, glm::vec3 maxp, float& distHit);
		bool IntersectTriangle(glm::vec3 orig, glm::vec3 dir, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float& distHit);
	}
}