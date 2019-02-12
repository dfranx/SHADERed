#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <MoonLight/Base/Image.h>
#include <MoonLight/Base/Texture.h>
#include <MoonLight/Base/ShaderResourceView.h>

#include "PipelineItem.h"
#include "ProjectParser.h"

namespace ed
{
	class RenderEngine;

	struct RenderTextureObject
	{
		ml::RenderTexture* RT;
		DirectX::XMINT2 FixedSize;
		DirectX::XMFLOAT2 RatioSize;
		ml::Color ClearColor;
	};

	class ObjectManager
	{
	public:
		ObjectManager(ml::Window* wnd, ProjectParser* parser, RenderEngine* rnd);
		~ObjectManager();

		bool CreateRenderTexture(const std::string& name);
		void CreateTexture(const std::string& file);

		void Bind(const std::string& file, PipelineItem* pass);
		void Unbind(const std::string& file, PipelineItem* pass);
		void Remove(const std::string& file);
		int IsBound(const std::string& file, PipelineItem* pass);

		RenderTextureObject* GetRenderTexture(const std::string& name) { return m_rts[name]; }
		DirectX::XMINT2 GetRenderTextureSize(const std::string& name);
		inline bool IsRenderTexture(const std::string& name) { return m_rts.count(name) > 0; }
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

		std::unordered_map<std::string, RenderTextureObject*> m_rts;
		
		std::unordered_map<PipelineItem*, std::vector<ml::ShaderResourceView*>> m_binds;
	};
}