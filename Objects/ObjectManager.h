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

	class ObjectManager
	{
	public:
		ObjectManager(ProjectParser* parser, RenderEngine* rnd);
		~ObjectManager();

		bool CreateRenderTexture(const std::string& name);
		void CreateTexture(const std::string& file);
		bool CreateAudio(const std::string& file);
		bool CreateCubemap(const std::string& name, const std::string& left, const std::string& top, const std::string& front, const std::string& bottom, const std::string& right, const std::string& back);

		void Update(float delta);

		void Bind(const std::string& file, PipelineItem* pass);
		void Unbind(const std::string& file, PipelineItem* pass);
		void Remove(const std::string& file);
		int IsBound(const std::string& file, PipelineItem* pass);

		glm::ivec2 GetRenderTextureSize(const std::string& name);
		RenderTextureObject* GetRenderTexture(GLuint tex);
		inline bool IsRenderTexture(const std::string& name) { return m_rts.count(name) > 0; }
		inline bool IsCubeMap(const std::string& name) { return m_isCube.count(name) > 0 && m_isCube[name]; }
		inline bool IsAudio(const std::string& name) { return m_audioData.count(name) > 0; }
		inline bool IsAudioMuted(const std::string& name) { return m_audioMute[name]; }
		bool IsCubeMap(GLuint id);

		void ResizeRenderTexture(const std::string& name, glm::ivec2 size);

		void Clear();

		inline std::vector<std::string> GetObjects() { return m_items; }
		inline GLuint GetTexture(const std::string& file) { return m_texs[file]; }
		inline std::pair<int, int> GetImageSize(const std::string& file) { return m_imgSize[file]; }

		void Mute(const std::string& name);
		void Unmute(const std::string& name);

		inline std::vector<GLuint> GetBindList(PipelineItem* pass) {
			if (m_binds.count(pass) > 0) return m_binds[pass];
			return std::vector<GLuint>();
		}
		inline bool Exists(const std::string& name) { return std::count(m_items.begin(), m_items.end(), name) > 0; }

		inline std::vector<std::string> GetCubemapTextures(const std::string& name) {
			return m_cubemaps[name];
		}

	private:
		RenderEngine* m_renderer;
		ProjectParser* m_parser;

		std::vector<std::string> m_items;
		std::unordered_map<std::string, std::pair<int, int>> m_imgSize;
		std::unordered_map<std::string, GLuint> m_texs;
		std::unordered_map<std::string, bool> m_isCube;
		std::unordered_map<std::string, std::vector<std::string>> m_cubemaps;

		ed::AudioAnalyzer m_audioAnalyzer;
		float m_audioTempTexData[ed::AudioAnalyzer::SampleCount * 2];
		std::unordered_map<std::string, sf::SoundBuffer> m_audioData;
		std::unordered_map<std::string, sf::Sound> m_audioPlayer;
		std::unordered_map<std::string, bool> m_audioMute;

		std::unordered_map<std::string, RenderTextureObject*> m_rts;

		std::unordered_map<PipelineItem*, std::vector<GLuint>> m_binds;
	};
}