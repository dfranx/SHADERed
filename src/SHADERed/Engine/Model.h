#pragma once
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace ed {
	namespace eng {
		class Model {
		public:
			class Mesh {
			public:
				struct Vertex {
					glm::vec3 Position;
					glm::vec3 Normal;
					glm::vec2 TexCoords;
					glm::vec3 Tangent;
					glm::vec3 Binormal;
					glm::vec4 Color;
				};
				struct Texture {
					unsigned int ID;
					std::string Type;
				};

				std::string Name;

				std::vector<Vertex> Vertices;
				std::vector<unsigned int> Indices;
				std::vector<Texture> Textures;

				Mesh(const std::string& name, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const std::vector<Texture>& textures);

				void Draw(bool instanced = false, int iCount = 0);

				unsigned int VAO, VBO, EBO;

			private:
				void m_setup();
			};

			~Model();

			std::vector<Mesh> Meshes;
			std::string Directory;

			std::vector<std::string> GetMeshNames();
			bool LoadFromFile(const std::string& path);
			void Draw(bool instanced = false, int iCount = 0);
			void Draw(const std::string& mesh);

			inline glm::vec3 GetMinBound() { return m_minBound; }
			inline glm::vec3 GetMaxBound() { return m_maxBound; }

		private:
			void m_findBounds();

			glm::vec3 m_minBound, m_maxBound;
			void m_processNode(aiNode* node, const aiScene* scene);
			Model::Mesh m_processMesh(aiMesh* mesh, const aiScene* scene);
		};
	}
}