#pragma once
#include <glm/glm.hpp>

namespace ed
{
	namespace eng
	{
		class GeometryFactory
		{
		public:
			struct Vertex
			{
				glm::vec3 Position;
				glm::vec3 Normal;
				glm::vec2 UV;
			};

			static const int VertexCount[7];

			static unsigned int CreateCube(unsigned int& vbo, float sx, float sy, float sz);
			static unsigned int CreateCircle(unsigned int& vbo, float rx, float ry);
			static unsigned int CreatePlane(unsigned int& vbo, float sx, float sy);
			static unsigned int CreateSphere(unsigned int& vbo, float r);
			static unsigned int CreateTriangle(unsigned int& vbo, float s);
			static unsigned int CreateScreenQuadNDC(unsigned int& vbo);
		};
	}
}