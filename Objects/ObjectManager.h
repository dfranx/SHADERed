#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <MoonLight/Base/Image.h>
#include <MoonLight/Base/Texture.h>
#include <MoonLight/Base/ShaderResourceView.h>

#include "PipelineItem.h"

namespace ed
{
	class ObjectManager
	{
	public:
		ObjectManager(ml::Window* wnd);
		~ObjectManager();

		void LoadTexture(const std::string& file);
		void Bind(const std::string& file, PipelineItem* pass);
		void Unbind(const std::string& file, PipelineItem* pass);
		void Remove(const std::string& file);
		bool IsBound(const std::string& file, PipelineItem* pass);

		inline std::vector<std::string> GetObjects() { return m_items; }
		inline ml::ShaderResourceView* GetSRV(const std::string& file) { return m_srvs[file]; }
		inline ml::Image* GetImage(const std::string& file) { return m_imgs[file]; }

		inline std::vector<ml::ShaderResourceView*> GetBindList(PipelineItem* pass) {
			if (m_binds.count(pass) > 0) return m_binds[pass];
			return std::vector<ml::ShaderResourceView*>();
		}



	private:
		ml::Window* m_wnd;

		std::vector<std::string> m_items;
		std::unordered_map<std::string, ml::Image*> m_imgs;
		std::unordered_map<std::string, ml::Texture*> m_texs;
		std::unordered_map<std::string, ml::ShaderResourceView*> m_srvs;

		std::unordered_map<PipelineItem*, std::vector<ml::ShaderResourceView*>> m_binds;
	};
}