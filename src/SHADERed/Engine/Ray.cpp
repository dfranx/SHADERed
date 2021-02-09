#include <SHADERed/Engine/Ray.h>
#include <algorithm>

namespace ed {
	namespace ray {
		/* https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection */
		bool IntersectBox(glm::vec3 orig, glm::vec3 dir, glm::vec3 minp, glm::vec3 maxp, float& distHit)
		{
			dir = glm::normalize(dir);

			float tmin = (minp.x - orig.x) / dir.x;
			float tmax = (maxp.x - orig.x) / dir.x;

			if (tmin > tmax) std::swap(tmin, tmax);

			float tymin = (minp.y - orig.y) / dir.y;
			float tymax = (maxp.y - orig.y) / dir.y;

			if (tymin > tymax) std::swap(tymin, tymax);

			if ((tmin > tymax) || (tymin > tmax))
				return false;

			if (tymin > tmin)
				tmin = tymin;

			if (tymax < tmax)
				tmax = tymax;

			float tzmin = (minp.z - orig.z) / dir.z;
			float tzmax = (maxp.z - orig.z) / dir.z;

			if (tzmin > tzmax) std::swap(tzmin, tzmax);

			if ((tmin > tzmax) || (tzmin > tmax))
				return false;

			if (tzmin > tmin)
				tmin = tzmin;

			if (tzmax < tmax)
				tmax = tzmax;

			distHit = tmin;

			return true;
		}

		/* https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm */
		bool IntersectTriangle(glm::vec3 orig, glm::vec3 dir, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float& iPoint)
		{
			const float EPSILON = 0.0000001f;

			glm::vec3 edge1, edge2, h, s, q;
			float a, f, u, v;

			edge1 = v1 - v0;
			edge2 = v2 - v0;
			h = glm::cross(dir, edge2);
			a = glm::dot(edge1, h);

			if (a > -EPSILON && a < EPSILON)
				return false; // parallel

			f = 1.0f / a;
			s = orig - v0;
			u = f * glm::dot(s, h);

			if (u < 0.0f || u > 1.0f)
				return false;

			q = glm::cross(s, edge1);
			v = f * glm::dot(dir, q);

			if (v < 0.0f || u + v > 1.0f)
				return false;

			float t = f * glm::dot(edge2, q);
			if (t > EPSILON) // ray intersection
			{
				iPoint = t;
				return true;
			}

			return false;
		}
	}
}