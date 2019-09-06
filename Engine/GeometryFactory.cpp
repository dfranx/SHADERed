#include "GeometryFactory.h"
#include "../Objects/PipelineItem.h"

#include <GL/glew.h>
#if defined(__APPLE__)
	#include <OpenGL/gl.h>
#else
	#include <GL/gl.h>
#endif
#include <glm/gtc/constants.hpp>

namespace ed
{
	namespace eng
	{
		const int GeometryFactory::VertexCount[] = {
			36,		/* CUBE */
			6,		/* RECTANGLE */
			32 * 3,	/* CIRCLE */
			3,		/* TRIANGLE */
			20 * 20 * 6,/* SPHERE */
			6,		/* PLANE */
		};

		void generateFace(GLfloat* verts, float radius, float sx, float sy, int x, int y)
		{
			float phi = y * sy;
			float theta = x * sx;
			GLfloat pt1[8] = {
				(radius * sin(phi) * cos(theta)),
				(radius * sin(phi) * sin(theta)),
				(radius * cos(phi)),
				0,0,0,
				theta / glm::two_pi<float>(), phi / glm::pi<float>()
			};
			glm::vec3 n = glm::normalize(glm::vec3(pt1[0], pt1[1], pt1[2]));
			pt1[3] = n.x; pt1[4] = n.y; pt1[5] = n.z;
			memcpy(verts + 2 * 8, pt1, sizeof(GLfloat) * 8);

			phi = y * sy;
			theta = (x + 1) * sx;
			GLfloat pt2[8] = {
				(radius * sin(phi) * cos(theta)),
				(radius * sin(phi) * sin(theta)),
				(radius * cos(phi)),
				0,0,0,
				theta / glm::two_pi<float>(), phi / glm::pi<float>()
			};
			n = glm::normalize(glm::vec3(pt2[0], pt2[1], pt2[2]));
			pt2[3] = n.x; pt2[4] = n.y; pt2[5] = n.z;
			memcpy(verts + 1 * 8, pt2, sizeof(GLfloat) * 8);

			phi = (y + 1) * sy;
			theta = x * sx;
			GLfloat pt3[8] = {
				(radius * sin(phi) * cos(theta)),
				(radius * sin(phi) * sin(theta)),
				(radius * cos(phi)),
				0,0,0,
				theta / glm::two_pi<float>(), phi / glm::pi<float>()
			};
			n = glm::normalize(glm::vec3(pt3[0], pt3[1], pt3[2]));
			pt3[3] = n.x; pt3[4] = n.y; pt3[5] = n.z;
			memcpy(verts + 0 * 8, pt3, sizeof(GLfloat) * 8);

			phi = y * sy;
			theta = (x + 1) * sx;
			GLfloat pt4[8] = {
				(radius * sin(phi) * cos(theta)),
				(radius * sin(phi) * sin(theta)),
				(radius * cos(phi)),
				0,0,0,
				theta / glm::two_pi<float>(), phi / glm::pi<float>()
			};
			n = glm::normalize(glm::vec3(pt4[0], pt4[1], pt4[2]));
			pt4[3] = n.x; pt4[4] = n.y; pt4[5] = n.z;
			memcpy(verts + 3 * 8, pt4, sizeof(GLfloat) * 8);

			phi = (y + 1) * sy;
			theta = x * sx;
			GLfloat pt5[8] = {
				(radius * sin(phi) * cos(theta)),
				(radius * sin(phi) * sin(theta)),
				(radius * cos(phi)),
				0,0,0,
				theta / glm::two_pi<float>(), phi / glm::pi<float>()
			};
			n = glm::normalize(glm::vec3(pt5[0], pt5[1], pt5[2]));
			pt5[3] = n.x; pt5[4] = n.y; pt5[5] = n.z;
			memcpy(verts + 4 * 8, pt5, sizeof(GLfloat) * 8);

			phi = (y + 1) * sy;
			theta = (x + 1) * sx;
			GLfloat pt6[8] = {
				(radius * sin(phi) * cos(theta)),
				(radius * sin(phi) * sin(theta)),
				(radius * cos(phi)),
				0,0,0,
				theta / glm::two_pi<float>(), phi / glm::pi<float>()
			};
			n = glm::normalize(glm::vec3(pt6[0], pt6[1], pt6[2]));
			pt6[3] = n.x; pt6[4] = n.y; pt6[5] = n.z;
			memcpy(verts + 5 * 8, pt6, sizeof(GLfloat) * 8);
		}

		unsigned int GeometryFactory::CreateCube(unsigned int& vbo, float sx, float sy, float sz)
		{
			float halfX = sx / 2.0f;
			float halfY = sy / 2.0f;
			float halfZ = sz / 2.0f;

			const GLfloat cubeData[] = {
				// front face
				-halfX, -halfY, halfZ, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // vec3, vec3, vec2
				halfX, -halfY, halfZ, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
				halfX, halfY, halfZ, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
				-halfX, -halfY, halfZ, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
				halfX, halfY, halfZ, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
				-halfX, halfY, halfZ, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,

				// back face
				halfX, halfY, -halfZ, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
				halfX, -halfY, -halfZ, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,
				-halfX, -halfY, -halfZ, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
				-halfX, halfY, -halfZ, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,
				halfX, halfY, -halfZ, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
				-halfX, -halfY, -halfZ, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,

				// right face
				halfX, -halfY, halfZ, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
				halfX, -halfY, -halfZ, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
				halfX, halfY, -halfZ, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
				halfX, -halfY, halfZ, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
				halfX, halfY, -halfZ, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
				halfX, halfY, halfZ, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,

				// left face
				-halfX, halfY, -halfZ, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
				-halfX, -halfY, -halfZ, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
				-halfX, -halfY, halfZ, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
				-halfX, halfY, halfZ, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
				-halfX, halfY, -halfZ, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
				-halfX, -halfY, halfZ, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,

				// top face
				-halfX, halfY, halfZ, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
				halfX, halfY, halfZ, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
				halfX, halfY, -halfZ, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
				-halfX, halfY, halfZ, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
				halfX, halfY, -halfZ, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
				-halfX, halfY, -halfZ, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,

				// bottom face
				halfX, -halfY, -halfZ, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,
				halfX, -halfY, halfZ, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
				-halfX, -halfY, halfZ, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
				-halfX, -halfY, -halfZ, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,
				halfX, -halfY, -halfZ, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,
				-halfX, -halfY, halfZ, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			};

			GLuint vao;

			// create vao
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);

			// create vbo
			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);

			// vbo data
			glBufferData(GL_ARRAY_BUFFER, 6 * 6 * 8 * sizeof(GLfloat), cubeData, GL_STATIC_DRAW);

			// vertex positions
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
			glEnableVertexAttribArray(0);

			// vertex normals
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
			glEnableVertexAttribArray(1);

			// vertex texture coords
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
			glEnableVertexAttribArray(2);

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			return vao;
		}
		unsigned int GeometryFactory::CreateCircle(unsigned int& vbo, float rx, float ry)
		{
			const int numPoints = 32 * 3;
			int numSegs = numPoints / 3;

			GLfloat circleData[numPoints * 8];


			float step = glm::two_pi<float>() / numSegs;

			for (int i = 0; i < numSegs; i++)
			{
				int j = i * 3 * 8;
				GLfloat* ptrData = &circleData[j];

				float xVal1 = sin(step * i);
				float yVal1 = cos(step * i);
				float xVal2 = sin(step * (i + 1));
				float yVal2 = cos(step * (i + 1));

				GLfloat point1[8] = { 0, 0, 0, 0, 0, 1, 0.5f, 0.5f };
				GLfloat point2[8] = { xVal1 * rx, yVal1 * ry, 0, 0, 0, 1, xVal1 * 0.5f + 0.5f, yVal1 * 0.5f + 0.5f };
				GLfloat point3[8] = { xVal2 * rx, yVal2 * ry, 0, 0, 0, 1, xVal2 * 0.5f + 0.5f, yVal2 * 0.5f + 0.5f };

				memcpy(ptrData + 0, point1, 8 * sizeof(GLfloat));
				memcpy(ptrData + 8, point2, 8 * sizeof(GLfloat));
				memcpy(ptrData + 16, point3, 8 * sizeof(GLfloat));
			}

			GLuint vao;

			// create vao
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);

			// create vbo
			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);

			// vbo data
			glBufferData(GL_ARRAY_BUFFER, numPoints * 8 * sizeof(GLfloat), circleData, GL_STATIC_DRAW);

			// vertex positions
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
			glEnableVertexAttribArray(0);

			// vertex normals
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
			glEnableVertexAttribArray(1);

			// vertex texture coords
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
			glEnableVertexAttribArray(2);

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			return vao;
		}
		unsigned int GeometryFactory::CreatePlane(unsigned int& vbo, float sx, float sy)
		{
			float halfX = sx / 2;
			float halfY = sy / 2;

			const GLfloat planeData[] = {
				halfX, halfY, 0, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
				halfX, -halfY, 0, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
				-halfX, -halfY, 0, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
				-halfX, halfY, 0, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
				halfX, halfY, 0, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
				-halfX, -halfY, 0, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			};

			GLuint vao;

			// create vao
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);

			// create vbo
			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);

			// vbo data
			glBufferData(GL_ARRAY_BUFFER, 6 * 6 * 8 * sizeof(GLfloat), planeData, GL_STATIC_DRAW);

			// vertex positions
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
			glEnableVertexAttribArray(0);

			// vertex normals
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
			glEnableVertexAttribArray(1);

			// vertex texture coords
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
			glEnableVertexAttribArray(2);

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			return vao;
		}
		unsigned int GeometryFactory::CreateSphere(unsigned int& vbo, float r)
		{
			const size_t stackCount = 20;
			const size_t sliceCount = 20;

			const size_t count = sliceCount * stackCount * 6;
			GLfloat sphereData[count * 8];

			const float stepY = glm::pi<float>() / stackCount;
			const float stepX = glm::two_pi<float>() / sliceCount;

			for (int i = 0; i < stackCount; i++) {
				float phi = i * stepY;
				for (int j = 0; j < sliceCount; j++) {
					float theta = j * stepX;
					size_t index = (i * sliceCount + j) * 6;

					generateFace(sphereData + index * 8, r, stepX, stepY, j, i);

				}
			}

			GLuint vao;

			// create vao
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);

			// create vbo
			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);

			// vbo data
			glBufferData(GL_ARRAY_BUFFER, count * 8 * sizeof(GLfloat), sphereData, GL_STATIC_DRAW);

			// vertex positions
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
			glEnableVertexAttribArray(0);

			// vertex normals
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
			glEnableVertexAttribArray(1);

			// vertex texture coords
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
			glEnableVertexAttribArray(2);

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			return vao;
		}
		unsigned int GeometryFactory::CreateTriangle(unsigned int& vbo, float s)
		{
			float rightOffs = s / tan(glm::radians(30.0f));
			const GLfloat triData[] = {
				-rightOffs, -s, 0, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
				rightOffs, -s, 0, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
				0, s, 0, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f,
			};

			GLuint vao;

			// create vao
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);

			// create vbo
			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);

			// vbo data
			glBufferData(GL_ARRAY_BUFFER, 3 * 8 * sizeof(GLfloat), triData, GL_STATIC_DRAW);

			// vertex positions
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
			glEnableVertexAttribArray(0);

			// vertex normals
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
			glEnableVertexAttribArray(1);

			// vertex texture coords
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
			glEnableVertexAttribArray(2);

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			return vao;
		}
	}
}