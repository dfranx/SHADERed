#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <MoonLight/Base/Image.h>
#include <MoonLight/Base/Texture.h>
#include <MoonLight/Audio/AudioFile.h>
#include <MoonLight/Audio/AudioPlayer.h>
#include <MoonLight/Base/ShaderResourceView.h>

#include "PipelineItem.h"
#include "ProjectParser.h"
#include "AudioAnalyzer.h"

namespace ed
{
	class RenderEngine;

	struct RenderTextureObject
	{
		ml::RenderTexture* RT;
		DirectX::XMINT2 FixedSize;
		DirectX::XMFLOAT2 RatioSize;
		ml::Color ClearColor;
		std::string Name;

		DirectX::XMINT2 CalculateSize(int w, int h)
		{
			DirectX::XMINT2 rtSize = FixedSize;
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
		ObjectManager(ml::Window* wnd, ProjectParser* parser, RenderEngine* rnd);
		~ObjectManager();

		bool CreateRenderTexture(const std::string& name);
		void CreateTexture(const std::string& file, bool cube = false);
		bool CreateAudio(const std::string& file);

		void Update(float delta);

		void Bind(const std::string& file, PipelineItem* pass);
		void Unbind(const std::string& file, PipelineItem* pass);
		void Remove(const std::string& file);
		int IsBound(const std::string& file, PipelineItem* pass);

		DirectX::XMINT2 GetRenderTextureSize(const std::string& name);
		inline RenderTextureObject* GetRenderTexture(const std::string& name) { return m_rts[name]; }
		inline bool IsRenderTexture(const std::string& name) { return m_rts.count(name) > 0; }
		inline bool IsAudio(const std::string& name) { return m_audioData.count(name) > 0; }
		inline bool IsCubeMap(const std::string& name) { return m_isCube.count(name) > 0 && m_isCube[name]; }
		inline bool IsLoading(const std::string& name) { return m_audioData.count(name) > 0 && (m_audioData[name]->IsLoading() || m_audioData[name]->HasFailed()); }
		void ResizeRenderTexture(const std::string& name, DirectX::XMINT2 size);

		void Clear();

		inline std::vector<std::string> GetObjects() { return m_items; }
		inline ml::ShaderResourceView* GetSRV(const std::string& file) { return m_srvs[file]; }
		inline ml::Image* GetImage(const std::string& file) { return m_imgs[file]; }

		inline std::vector<ml::ShaderResourceView*> GetBindList(PipelineItem* pass) {
			if (m_binds.count(pass) > 0) return m_binds[pass];
			return std::vector<ml::ShaderResourceView*>();
		}



	private:
		ml::Window* m_wnd;
		RenderEngine* m_renderer;
		ProjectParser* m_parser;

		std::vector<std::string> m_items;
		std::unordered_map<std::string, ml::Image*> m_imgs;
		std::unordered_map<std::string, ml::Texture*> m_texs;
		std::unordered_map<std::string, ml::ShaderResourceView*> m_srvs;
		std::unordered_map<std::string, bool> m_isCube;

		std::unordered_map<std::string, RenderTextureObject*> m_rts;

		ml::AudioEngine m_audioEngine;
		ed::AudioAnalyzer m_audioAnalyzer;
		float m_audioTempTexData[ed::AudioAnalyzer::SampleCount * 2];

		std::unordered_map<std::string, ml::AudioFile*> m_audioData;
		std::unordered_map<std::string, ml::AudioPlayer*> m_audioPlayer;
		std::unordered_map<std::string, int> m_audioExclude; // how many loops have we played already
		
		std::unordered_map<PipelineItem*, std::vector<ml::ShaderResourceView*>> m_binds;
	};
}