#include "ObjectManager.h"

namespace ed
{
	ml::Image::Type mGetImgType(const std::string& fname)
	{
		ml::Image::Type type = ml::Image::Type::WIC;

		std::size_t lastDot = fname.find_last_of('.');
		std::string ext = fname.substr(lastDot + 1, fname.size() - (lastDot + 1));

		std::transform(ext.begin(), ext.end(), ext.begin(), tolower);

		if (ext == "dds")
			type = ml::Image::Type::DDS;
		else if (ext == "tga")
			type = ml::Image::Type::TGA;
		else if (ext == "hdr")
			type = ml::Image::Type::HDR;

		return type;
	}

	ObjectManager::ObjectManager(ml::Window * wnd, ProjectParser* parser) :
		m_parser(parser)
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
		
		size_t imgDataLen = 0;
		char* imgData = m_parser->LoadProjectFile(file, imgDataLen);
		if (img->LoadFromMemory(imgData, imgDataLen, mGetImgType(file))) {
			tex->Create(*m_wnd, *img);
			srv->Create(*m_wnd, *tex);
		}
		free(imgData);
	}
	void ObjectManager::Bind(const std::string & file, PipelineItem * pass)
	{
		if (IsBound(file, pass) == -1)
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
	int ObjectManager::IsBound(const std::string & file, PipelineItem * pass)
	{
		if (m_binds.count(pass) == 0)
			return -1;

		for (int i = 0; i < m_binds[pass].size(); i++)
			if (m_binds[pass][i] == m_srvs[file]) {
				return i;
			}

		return -1;
	}
}