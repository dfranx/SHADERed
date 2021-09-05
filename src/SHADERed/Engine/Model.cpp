#include <SHADERed/Engine/Model.h>
#include <SHADERed/Objects/Logger.h>

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <iostream>

namespace ed {
	namespace eng {
		Model::Mesh::Mesh(const std::string& name, const std::vector<Model::Mesh::Vertex>& vertices, const std::vector<unsigned int>& indices, const std::vector<Model::Mesh::Texture>& textures)
		{
			Name = name;
			Vertices = vertices;
			Indices = indices;
			Textures = textures;
			m_setup();
		}
		void Model::Mesh::m_setup()
		{
			glGenVertexArrays(1, &VAO);
			glGenBuffers(1, &VBO);
			glGenBuffers(1, &EBO);
			glBindVertexArray(VAO);

			glBindBuffer(GL_ARRAY_BUFFER, VBO);
			glBufferData(GL_ARRAY_BUFFER, Vertices.size() * sizeof(Vertex), &Vertices[0],
				GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, Indices.size() * sizeof(unsigned int),
				&Indices[0], GL_STATIC_DRAW);

			// vertex positions
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Model::Mesh::Vertex), (void*)0);
			glEnableVertexAttribArray(0);

			// vertex normals
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Model::Mesh::Vertex), (void*)offsetof(Vertex, Normal));
			glEnableVertexAttribArray(1);

			// vertex texture coords
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Model::Mesh::Vertex), (void*)offsetof(Vertex, TexCoords));
			glEnableVertexAttribArray(2);

			glBindVertexArray(0);
		}
		void Model::Mesh::Draw(bool instanced, int iCount)
		{
			// draw mesh
			glBindVertexArray(VAO);

			if (instanced)
				glDrawElementsInstanced(GL_TRIANGLES, Indices.size(), GL_UNSIGNED_INT, nullptr, iCount);
			else
				glDrawElements(GL_TRIANGLES, Indices.size(), GL_UNSIGNED_INT, 0);
		}

		Model::~Model()
		{
			for (int i = 0; i < Meshes.size(); i++) {
				glDeleteVertexArrays(1, &Meshes[i].VAO);
				glDeleteBuffers(1, &Meshes[i].VBO);
				glDeleteBuffers(1, &Meshes[i].EBO);
			}
		}

		bool Model::LoadFromFile(const std::string& path)
		{
			ed::Logger::Get().Log("Loading a 3D model from file \"" + path + "\"");

			// read file via ASSIMP
			Assimp::Importer importer;
			const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

			// check for errors
			if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
			{
				ed::Logger::Get().Log("Assimp has detected an error \"" + std::string(importer.GetErrorString()) + "\"", true);
				return false;
			}

			Directory = path.substr(0, path.find_last_of("/\\"));
			m_processNode(scene->mRootNode, scene);

			m_findBounds();

			return true;
		}
		void Model::m_findBounds()
		{
			m_minBound = glm::vec3(std::numeric_limits<float>::infinity());
			m_maxBound = glm::vec3(-std::numeric_limits<float>::infinity());

			for (auto& mesh : Meshes) {
				for (auto& v : mesh.Vertices) {
					m_minBound.x = std::min<float>(m_minBound.x, v.Position.x);
					m_minBound.y = std::min<float>(m_minBound.y, v.Position.y);
					m_minBound.z = std::min<float>(m_minBound.z, v.Position.z);
					m_maxBound.x = std::max<float>(m_maxBound.x, v.Position.x);
					m_maxBound.y = std::max<float>(m_maxBound.y, v.Position.y);
					m_maxBound.z = std::max<float>(m_maxBound.z, v.Position.z);
				}
			}
		}
		std::vector<std::string> Model::GetMeshNames()
		{
			std::vector<std::string> ret;
			for (int i = 0; i < Meshes.size(); i++)
				ret.push_back(Meshes[i].Name);
			return ret;
		}
		void Model::Draw(bool inst, int iCount)
		{
			for (unsigned int i = 0; i < Meshes.size(); i++)
				Meshes[i].Draw(inst, iCount);
		}
		void Model::Draw(const std::string& mesh)
		{
			for (unsigned int i = 0; i < Meshes.size(); i++)
				if (Meshes[i].Name == mesh)
					Meshes[i].Draw();
		}
		void Model::m_processNode(aiNode* node, const aiScene* scene)
		{
			for (unsigned int i = 0; i < node->mNumMeshes; i++) {
				aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
				Meshes.push_back(m_processMesh(mesh, scene));
			}

			for (unsigned int i = 0; i < node->mNumChildren; i++)
				m_processNode(node->mChildren[i], scene);
		}
		Model::Mesh Model::m_processMesh(aiMesh* mesh, const aiScene* scene)
		{
			// data to fill
			std::vector<Model::Mesh::Vertex> vertices(mesh->mNumVertices);
			std::vector<unsigned int> indices;
			std::vector<Model::Mesh::Texture> textures;

			// walk through each of the mesh's vertices
			for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
				Model::Mesh::Vertex& vertex = vertices[i];
				glm::vec3 vector;

				// positions
				if (mesh->HasPositions()) {
					vector.x = mesh->mVertices[i].x;
					vector.y = mesh->mVertices[i].y;
					vector.z = mesh->mVertices[i].z;
					vertex.Position = vector;
				} else
					vertex.Position = glm::vec3(0, 0, 0);

				// normals
				if (mesh->HasNormals()) {
					vector.x = mesh->mNormals[i].x;
					vector.y = mesh->mNormals[i].y;
					vector.z = mesh->mNormals[i].z;
					vertex.Normal = vector;
				} else
					vertex.Normal = glm::vec3(0, 0, 0);

				// texture coordinates
				if (mesh->HasTextureCoords(0)) // does the mesh contain texture coordinates?
				{
					glm::vec2 vec;
					vec.x = mesh->mTextureCoords[0][i].x;
					vec.y = mesh->mTextureCoords[0][i].y;
					vertex.TexCoords = vec;
				} else
					vertex.TexCoords = glm::vec2(0.0f, 0.0f);

				// tangents and binormals
				if (mesh->HasTangentsAndBitangents()) {
					vector.x = mesh->mTangents[i].x;
					vector.y = mesh->mTangents[i].y;
					vector.z = mesh->mTangents[i].z;
					vertex.Tangent = vector;

					vector.x = mesh->mBitangents[i].x;
					vector.y = mesh->mBitangents[i].y;
					vector.z = mesh->mBitangents[i].z;
					vertex.Binormal = vector;
				} else {
					vertex.Tangent = glm::vec3(0, 0, 0);
					vertex.Binormal = glm::vec3(0, 0, 0);
				}

				// texture coordinates
				if (mesh->HasVertexColors(0)) {
					glm::vec4 vec;
					vec.x = mesh->mColors[0][i].r;
					vec.y = mesh->mColors[0][i].g;
					vec.z = mesh->mColors[0][i].b;
					vec.w = mesh->mColors[0][i].a;
					vertex.Color = vec;
				} else
					vertex.Color = glm::vec4(1, 1, 1, 1);
			}

			// now walk through each of the mesh's faces
			for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
				aiFace face = mesh->mFaces[i];
				for (unsigned int j = 0; j < face.mNumIndices; j++)
					indices.push_back(face.mIndices[j]);
			}

			// process materials
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

			// TODO: textures

			// return a mesh object created from the extracted mesh data
			return Model::Mesh(mesh->mName.data, vertices, indices, textures);
		}
	}
}