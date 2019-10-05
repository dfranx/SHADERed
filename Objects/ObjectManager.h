#pragma once
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <SDL2/SDL_surface.h>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

#include "PipelineItem.h"
#include "ProjectParser.h"
#include "AudioAnalyzer.h"

namespace ed
{
	class RenderEngine;

	struct RenderTextureObject
	{
		GLuint DepthStencilBuffer; // ColorBuffer is stored in ObjectManager
		glm::ivec2 FixedSize;
		glm::vec2 RatioSize;
		glm::vec4 ClearColor;
		std::string Name;
		bool Clear;
		GLuint Format;

		RenderTextureObject() : FixedSize(-1, -1), RatioSize(1,1),
		Clear(true), ClearColor(0,0,0,1), Format(GL_RGBA) { }

		glm::ivec2 CalculateSize(int w, int h)
		{
			glm::ivec2 rtSize = FixedSize;
			if (rtSize.x == -1) {
				rtSize.x = RatioSize.x * w;
				rtSize.y = RatioSize.y * h;
			}

			return rtSize;
		}
	};

	struct BufferObject
	{
		int Size;
		void* Data;
		char ViewFormat[256]; // vec3;vec3;vec2
		GLuint ID;
	};

	struct ImageObject
	{
		glm::ivec2 Size;
		GLuint Format;
		GLuint Texture;
	};

	/* Use this to remove all the maps */
	class ObjectManagerItem
	{
	public:
		ObjectManagerItem() {
			ImageSize = glm::ivec2(0, 0);
			Texture = 0;
			IsCube = false;
			CubemapPaths.clear();
			SoundBuffer = nullptr;
			Sound = nullptr;
			SoundMuted = false;
			RT = nullptr;
			Buffer = nullptr;
			Image = nullptr;
		}
		~ObjectManagerItem() {
			if (Buffer != nullptr) {
				glDeleteBuffers(1, &Buffer->ID);
				free(Buffer->Data);
				delete Buffer;
			}
			else if (Image != nullptr) {
				glDeleteTextures(1, &Image->Texture);
				delete Image;
			}

			if (RT != nullptr) {
				glDeleteTextures(1, &RT->DepthStencilBuffer);
				delete RT;
			}
			else if (Sound != nullptr) {
				if (Sound->getStatus() == sf::Sound::Playing)
					Sound->stop();

				delete SoundBuffer;
				delete Sound;
			}


			if (Texture != 0)
				glDeleteTextures(1, &Texture);
			CubemapPaths.clear();
		}

		glm::ivec2 ImageSize;
		GLuint Texture;
		bool IsCube;
		std::vector<std::string> CubemapPaths;
		
		sf::SoundBuffer* SoundBuffer;
		sf::Sound* Sound;
		bool SoundMuted;

		RenderTextureObject* RT;
		BufferObject* Buffer;
		ImageObject* Image;
	};

	class ObjectManager
	{
	public:
		ObjectManager(ProjectParser* parser, RenderEngine* rnd);
		~ObjectManager();

		bool CreateRenderTexture(const std::string& name);
		bool CreateTexture(const std::string& file);
		bool CreateAudio(const std::string& file);
		bool CreateCubemap(const std::string& name, const std::string& left, const std::string& top, const std::string& front, const std::string& bottom, const std::string& right, const std::string& back);
		bool CreateBuffer(const std::string& file);
		bool CreateImage(const std::string& name, glm::ivec2 size = glm::ivec2(1,1));

		void Update(float delta);

		void Remove(const std::string& file);
		
		glm::ivec2 GetRenderTextureSize(const std::string& name);
		RenderTextureObject* GetRenderTexture(GLuint tex);
		bool IsRenderTexture(const std::string& name);
		bool IsCubeMap(const std::string& name);
		bool IsAudio(const std::string& name);
		bool IsAudioMuted(const std::string& name);
		bool IsBuffer(const std::string& name);
		bool IsImage(const std::string& name);
		bool IsImage(GLuint id);
		bool IsCubeMap(GLuint id);

		void ResizeRenderTexture(const std::string& name, glm::ivec2 size);
		void ResizeImage(const std::string& name, glm::ivec2 size);

		void Clear();

		const std::vector<std::string>& GetObjects() { return m_items; }
		GLuint GetTexture(const std::string& file);
		glm::ivec2 GetTextureSize(const std::string& file);
		sf::SoundBuffer* GetSoundBuffer(const std::string& file);
		sf::Sound* GetAudioPlayer(const std::string& file);
		BufferObject* GetBuffer(const std::string& name);
		ImageObject* GetImage(const std::string& name);
		RenderTextureObject* GetRenderTexture(const std::string& name);
		glm::ivec2 GetImageSize(const std::string& name);

		std::string GetBufferNameByID(int id);
		std::string GetImageNameByID(GLuint id);
		
		void Mute(const std::string& name);
		void Unmute(const std::string& name);

		std::string GetItemNameByTextureID(GLuint texID);

		std::vector<ed::ShaderVariable::ValueType> ParseBufferFormat(const std::string& str);

		void Bind(const std::string& file, PipelineItem* pass);
		void Unbind(const std::string& file, PipelineItem* pass);
		int IsBound(const std::string& file, PipelineItem* pass);
		inline std::vector<GLuint>& GetBindList(PipelineItem* pass) {
			if (m_binds.count(pass) > 0) return m_binds[pass];
			return m_emptyResVec;
		}

		void BindUniform(const std::string& file, PipelineItem* pass);
		void UnbindUniform(const std::string& file, PipelineItem* pass);
		int IsUniformBound(const std::string& file, PipelineItem* pass);
		inline std::vector<GLuint>& GetUniformBindList(PipelineItem* pass) {
			if (m_uniformBinds.count(pass) > 0) return m_uniformBinds[pass];
			return m_emptyResVec;
		}

		inline bool Exists(const std::string& name) { return std::count(m_items.begin(), m_items.end(), name) > 0; }

		const std::vector<std::string>& GetCubemapTextures(const std::string& name);
		inline std::vector<ObjectManagerItem*> GetItemDataList() { return m_itemData; }

	private:
		RenderEngine* m_renderer;
		ProjectParser* m_parser;

		std::vector<std::string> m_items; // TODO: move item name to item data
		std::vector<ObjectManagerItem*> m_itemData; 

		std::vector<GLuint> m_emptyResVec;
		std::vector<std::string> m_emptyCBTexs;

		ed::AudioAnalyzer m_audioAnalyzer;
		float m_audioTempTexData[ed::AudioAnalyzer::SampleCount * 2];

		std::unordered_map<PipelineItem*, std::vector<GLuint>> m_binds;
		std::unordered_map<PipelineItem*, std::vector<GLuint>> m_uniformBinds;
	};
}