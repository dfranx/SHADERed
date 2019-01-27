#include "ObjectManager.h"

namespace ed
{
	ObjectManager::ObjectManager(ml::Window * wnd)
	{
		m_wnd = wnd;
	}
	ObjectManager::~ObjectManager()
	{
		for (auto str : m_items) {
			delete m_imgs[str];
			delete m_srvs[str];
			delete m_texs[str];
		}
	}
	void ObjectManager::LoadTexture(const std::string& file)
	{
		ml::Image* img = (m_imgs[file] = new ml::Image());
		ml::Texture* tex = (m_texs[file] = new ml::Texture());
		ml::ShaderResourceView* srv = (m_srvs[file] = new ml::ShaderResourceView());

		m_items.push_back(file);

		if (img->LoadFromFile(file)) {
			tex->Create(*m_wnd, *img);
			srv->Create(*m_wnd, *tex);
		}
		
	}
	void ObjectManager::Bind(const std::string & file, PipelineItem * pass)
	{
		if (!this->IsBound(file, pass))
			m_binds[pass].push_back(m_srvs[file]);
	}
	void ObjectManager::Unbind(const std::string & file, PipelineItem * pass)
	{
		std::vector<ml::ShaderResourceView*>& srvs = m_binds[pass];
		ml::ShaderResourceView* srv = m_srvs[file];

		for (int i = 0; i < srvs.size(); i++)
			if (srvs[i] == srv) {
				srvs.erase(srvs.begin() + i);
				return;
			}
	}
	void ObjectManager::Remove(const std::string & file)
	{
		for (auto i : m_binds)
			for (int j = 0; j < i.second.size(); j++)
				if (i.second[j] == m_srvs[file]) {
					i.second.erase(i.second.begin() + j);
					j--;
				}

		delete m_srvs[file];
		delete m_texs[file];
		delete m_imgs[file];

		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == file) {
				m_items.erase(m_items.begin() + i);
				break;
			}

		m_srvs.erase(file);
		m_texs.erase(file);
		m_imgs.erase(file);
	}
	bool ObjectManager::IsBound(const std::string & file, PipelineItem * pass)
	{
		if (m_binds.count(pass) == 0)
			return false;

		bool exists = false;
		for (int i = 0; i < m_binds[pass].size(); i++)
			if (m_binds[pass][i] == m_srvs[file]) {
				exists = true;
				break;
			}

		return exists;
	}
}