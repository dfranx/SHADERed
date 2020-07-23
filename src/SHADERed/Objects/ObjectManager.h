#pragma once
#include <SDL2/SDL_surface.h>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SHADERed/Objects/AudioAnalyzer.h>
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/ProjectParser.h>

namespace ed {
	class RenderEngine;

	struct RenderTextureObject {
		GLuint DepthStencilBuffer, DepthStencilBufferMS, BufferMS; // ColorBuffer is stored in ObjectManager
		glm::ivec2 FixedSize;
		glm::vec2 RatioSize;
		glm::vec4 ClearColor;
		std::string Name;
		bool Clear;
		GLuint Format;

		RenderTextureObject()
				: FixedSize(-1, -1)
				, RatioSize(1, 1)
				, Clear(true)
				, ClearColor(0, 0, 0, 1)
				, Format(GL_RGBA)
		{
		}

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

	struct BufferObject {
		int Size;
		void* Data;
		char ViewFormat[256]; // vec3;vec3;vec2
		GLuint ID;
		bool PreviewPaused;
	};

	struct ImageObject {
		glm::ivec2 Size;
		GLuint Format;
		GLuint Texture;
		char DataPath[SHADERED_MAX_PATH];
	};

	struct Image3DObject {
		glm::ivec3 Size;
		GLuint Format;
		GLuint Texture;
	};

	struct PluginObject {
		char Type[128];
		IPlugin1* Owner;

		GLuint ID;
		void* Data;
	};

	/* Use this to remove all the maps */
	class ObjectManagerItem {
	public:
		ObjectManagerItem()
		{
			ImageSize = glm::ivec2(0, 0);
			Texture = 0;
			FlippedTexture = 0;
			IsCube = false;
			IsTexture = false;
			IsKeyboardTexture = false;
			Texture_VFlipped = false;
			Texture_MinFilter = GL_LINEAR;
			Texture_MagFilter = GL_NEAREST;
			Texture_WrapS = GL_REPEAT;
			Texture_WrapT = GL_REPEAT;
			CubemapPaths.clear();
			SoundBuffer = nullptr;
			Sound = nullptr;
			SoundMuted = false;
			RT = nullptr;
			Buffer = nullptr;
			Image = nullptr;
			Image3D = nullptr;
			Plugin = nullptr;
		}
		~ObjectManagerItem()
		{
			if (Buffer != nullptr) {
				glDeleteBuffers(1, &Buffer->ID);
				free(Buffer->Data);
				delete Buffer;
			}
			if (Image != nullptr) {
				glDeleteTextures(1, &Image->Texture);
				delete Image;
			}
			if (Image3D != nullptr) {
				glDeleteTextures(1, &Image3D->Texture);
				delete Image3D;
			}

			if (RT != nullptr) {
				glDeleteTextures(1, &RT->DepthStencilBuffer);
				delete RT;
			}
			if (Sound != nullptr) {
				if (Sound->getStatus() == sf::Sound::Playing)
					Sound->stop();

				delete SoundBuffer;
				delete Sound;
			}
			if (Plugin != nullptr)
				delete Plugin;

			glDeleteTextures(1, &Texture);
			glDeleteTextures(1, &FlippedTexture);
			CubemapPaths.clear();
		}

		glm::ivec2 ImageSize;
		GLuint Texture, FlippedTexture;
		bool IsCube;
		bool IsTexture;
		bool IsKeyboardTexture;
		std::vector<std::string> CubemapPaths;

		bool Texture_VFlipped;
		GLuint Texture_MinFilter, Texture_MagFilter, Texture_WrapS, Texture_WrapT;

		sf::SoundBuffer* SoundBuffer;
		sf::Sound* Sound;
		bool SoundMuted;

		RenderTextureObject* RT;
		BufferObject* Buffer;
		ImageObject* Image;
		Image3DObject* Image3D;

		PluginObject* Plugin;
	};

	class ObjectManager {
	public:
		ObjectManager(ProjectParser* parser, RenderEngine* rnd);
		~ObjectManager();

		bool CreateRenderTexture(const std::string& name);
		bool CreateTexture(const std::string& file);
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

		glm::ivec2 GetRenderTextureSize(const std::string& name);
		RenderTextureObject* GetRenderTexture(GLuint tex);
		bool IsRenderTexture(const std::string& name);
		bool IsCubeMap(const std::string& name);
		bool IsAudio(const std::string& name);
		bool IsAudioMuted(const std::string& name);
		bool IsBuffer(const std::string& name);
		bool IsImage(const std::string& name);
		bool IsTexture(const std::string& name);
		bool IsImage3D(const std::string& name);
		bool IsPluginObject(const std::string& name);
		bool IsPluginObject(GLuint id);
		bool IsImage3D(GLuint id);
		bool IsImage(GLuint id);
		bool IsCubeMap(GLuint id);

		void UploadDataToImage(ImageObject* img, GLuint tex, glm::ivec2 texSize);
		void SaveToFile(const std::string& itemName, ObjectManagerItem* item, const std::string& filepath);

		void ResizeRenderTexture(const std::string& name, glm::ivec2 size);
		void ResizeImage(const std::string& name, glm::ivec2 size);
		void ResizeImage3D(const std::string& name, glm::ivec3 size);

		bool LoadBufferFromTexture(BufferObject* buf, const std::string& str, bool convertToFloat = false);
		bool LoadBufferFromModel(BufferObject* buf, const std::string& str);
		bool LoadBufferFromFile(BufferObject* buf, const std::string& str);

		bool ReloadTexture(ObjectManagerItem* item, const std::string& newPath);

		void Clear();

		const std::vector<std::string>& GetObjects() { return m_items; }
		GLuint GetTexture(const std::string& file);
		GLuint GetFlippedTexture(const std::string& file);
		glm::ivec2 GetTextureSize(const std::string& file);
		sf::SoundBuffer* GetSoundBuffer(const std::string& file);
		sf::Sound* GetAudioPlayer(const std::string& file);
		BufferObject* GetBuffer(const std::string& name);
		ImageObject* GetImage(const std::string& name);
		Image3DObject* GetImage3D(const std::string& name);
		RenderTextureObject* GetRenderTexture(const std::string& name);
		PluginObject* GetPluginObject(const std::string& name);
		glm::ivec2 GetImageSize(const std::string& name);
		glm::ivec3 GetImage3DSize(const std::string& name);

		bool HasKeyboardTexture();

		PluginObject* GetPluginObject(GLuint id);
		std::string GetBufferNameByID(int id);
		std::string GetImageNameByID(GLuint id);
		std::string GetImage3DNameByID(GLuint id);

		ObjectManagerItem* GetObjectManagerItem(const std::string& name);
		std::string GetObjectManagerItemName(ObjectManagerItem* item);

		void Mute(const std::string& name);
		void Unmute(const std::string& name);

		std::string GetItemNameByTextureID(GLuint texID);

		std::vector<ed::ShaderVariable::ValueType> ParseBufferFormat(const std::string& str);

		void FlipTexture(const std::string& name);
		void UpdateTextureParameters(const std::string& name);

		void Bind(const std::string& file, PipelineItem* pass);
		void Unbind(const std::string& file, PipelineItem* pass);
		int IsBound(const std::string& file, PipelineItem* pass);
		inline std::vector<GLuint>& GetBindList(PipelineItem* pass)
		{
			if (m_binds.count(pass) > 0) return m_binds[pass];
			return m_emptyResVec;
		}

		void BindUniform(const std::string& file, PipelineItem* pass);
		void UnbindUniform(const std::string& file, PipelineItem* pass);
		int IsUniformBound(const std::string& file, PipelineItem* pass);
		inline std::vector<GLuint>& GetUniformBindList(PipelineItem* pass)
		{
			if (m_uniformBinds.count(pass) > 0) return m_uniformBinds[pass];
			return m_emptyResVec;
		}

		inline bool Exists(const std::string& name) { return std::count(m_items.begin(), m_items.end(), name) > 0; }

		const std::vector<std::string>& GetCubemapTextures(const std::string& name);
		inline std::vector<ObjectManagerItem*>& GetItemDataList() { return m_itemData; }

	private:
		RenderEngine* m_renderer;
		ProjectParser* m_parser;

		std::vector<std::string> m_items; // TODO: move item name to item data
		std::vector<ObjectManagerItem*> m_itemData;

		std::vector<GLuint> m_emptyResVec;
		std::vector<char> m_emptyResVecChar;
		std::vector<std::string> m_emptyCBTexs;

		ed::AudioAnalyzer m_audioAnalyzer;
		float m_audioTempTexData[ed::AudioAnalyzer::SampleCount * 2];

		unsigned char m_kbTexture[256 * 3];

		std::unordered_map<PipelineItem*, std::vector<GLuint>> m_binds;
		std::unordered_map<PipelineItem*, std::vector<GLuint>> m_uniformBinds;
	};
}