#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <SHADERed/Objects/InputLayout.h>

namespace ed {
	namespace eng {
		class GeometryFactory {
		public:
			struct Vertex {
				glm::vec3 Position;
				glm::vec3 Normal;
				glm::vec2 UV;
				glm::vec3 Tangent;
				glm::vec3 Binormal;
				glm::vec4 Color;
			};

			static const int VertexCount[7];

			static unsigned int CreateCube(unsigned int& vbo, float sx, float sy, float sz, const std::vector<InputLayoutItem>& inp);
			static unsigned int CreateCircle(unsigned int& vbo, float rx, float ry, const std::vector<InputLayoutItem>& inp);
			static unsigned int CreatePlane(unsigned int& vbo, float sx, float sy, const std::vector<InputLayoutItem>& inp);
			static unsigned int CreateSphere(unsigned int& vbo, float r, const std::vector<InputLayoutItem>& inp);
			static unsigned int CreateTriangle(unsigned int& vbo, float s, const std::vector<InputLayoutItem>& inp);
			static unsigned int CreateScreenQuadNDC(unsigned int& vbo, const std::vector<InputLayoutItem>& inp);
		};
	}
}