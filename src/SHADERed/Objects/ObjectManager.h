#pragma once
#include <SDL2/SDL_surface.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SHADERed/Objects/AudioAnalyzer.h>
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/ProjectParser.h>
#include <SHADERed/Objects/ObjectManagerItem.h>

namespace ed {
	class RenderEngine;

	class ObjectManager {
	public:
		ObjectManager(ProjectParser* parser, RenderEngine* rnd);
		~ObjectManager();

		bool CreateRenderTexture(const std::string& name);
		bool CreateTexture(const std::string& file);
		bool CreateTexture3D(const std::string& file);
		bool CreateAudio(const std::string& file);
		bool CreateCubemap(const std::string& name, const std::string& left, const std::string& top, const std::string& front, const std::string& bottom, const std::string& right, const std::string& back);
		bool CreateBuffer(const std::string& file);
		bool CreateImage(const std::string& name, glm::ivec2 size = glm::ivec2(1, 1));
		bool CreateImage3D(const std::string& name, glm::ivec3 size = glm::ivec3(1, 1, 1));
		bool CreatePluginItem(const std::string& name, const std::string& objtype, void* data, GLuint id, IPlugin1* owner);
		bool CreateKeyboardTexture(const std::string& name);
		
		void OnEvent(const SDL_Event& e);
		void Update(float delta);

		void Pause(bool pause);

		void Remove(const std::string& file);

		glm::ivec2 GetRenderTextureSize(ObjectManagerItem* obj);

		void UploadDataToImage(ImageObject* img, GLuint tex, glm::ivec2 texSize);
		void SaveToFile(ObjectManagerItem* item, const std::string& filepath);

		void ResizeRenderTexture(ObjectManagerItem* item, glm::ivec2 size);
		void ResizeImage(ObjectManagerItem* item, glm::ivec2 size);
		void ResizeImage3D(ObjectManagerItem* item, glm::ivec3 size);

		bool LoadBufferFromTexture(BufferObject* buf, const std::string& str, bool convertToFloat = false);
		bool LoadBufferFromModel(BufferObject* buf, const std::string& str);
		bool LoadBufferFromFile(BufferObject* buf, const std::string& str);

		bool ReloadTexture(ObjectManagerItem* item, const std::string& newPath);

		void Clear();

		inline std::vector<ObjectManagerItem*>& GetObjects() { return m_items; }

		bool HasKeyboardTexture();

		ObjectManagerItem* GetByTextureID(GLuint tex);
		ObjectManagerItem* GetByBufferID(GLuint tex);
		ObjectManagerItem* Get(const std::string& name);

		void Mute(ObjectManagerItem* item);
		void Unmute(ObjectManagerItem* item);

		std::vector<ed::ShaderVariable::ValueType> ParseBufferFormat(const std::string& str);

		void FlipTexture(const std::string& name);
		void UpdateTextureParameters(const std::string& name);

		void Bind(ObjectManagerItem* item, PipelineItem* pass);
		void Unbind(ObjectManagerItem* item, PipelineItem* pass);
		int IsBound(ObjectManagerItem* item, PipelineItem* pass);
		inline std::vector<GLuint>& GetBindList(PipelineItem* pass)
		{
			if (m_binds.count(pass) > 0) return m_binds[pass];
			return m_emptyResVec;
		}

		void BindUniform(ObjectManagerItem* item, PipelineItem* pass);
		void UnbindUniform(ObjectManagerItem* item, PipelineItem* pass);
		int IsUniformBound(ObjectManagerItem* item, PipelineItem* pass);
		inline std::vector<GLuint>& GetUniformBindList(PipelineItem* pass)
		{
			if (m_uniformBinds.count(pass) > 0) return m_uniformBinds[pass];
			return m_emptyResVec;
		}

		bool Exists(const std::string& name);

	private:
		RenderEngine* m_renderer;
		ProjectParser* m_parser;

		std::vector<ObjectManagerItem*> m_items;

		std::unordered_map<SDL_Keycode, int> m_keyIDs;

		inline GLuint m_getGLObject(ObjectManagerItem* item)
		{
			GLuint data = item->Texture;
			if (item->Type == ObjectType::Buffer && item->Buffer != nullptr)
				data = item->Buffer->ID;
			else if (item->Type == ObjectType::PluginObject && item->Plugin != nullptr)
				data = item->Plugin->ID;

			return data;
		}

		std::vector<GLuint> m_emptyResVec;
		std::vector<char> m_emptyResVecChar;

		ed::AudioAnalyzer m_audioAnalyzer;
		float m_audioTempTexData[ed::AudioAnalyzer::SampleCount * 2];
		short m_samplesTempBuffer[1024];

		unsigned char m_kbTexture[256 * 3];

		std::unordered_map<PipelineItem*, std::vector<GLuint>> m_binds;
		std::unordered_map<PipelineItem*, std::vector<GLuint>> m_uniformBinds;
	};
}