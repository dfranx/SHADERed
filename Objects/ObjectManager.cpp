#include "ObjectManager.h"
#include "RenderEngine.h"

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

	ObjectManager::ObjectManager(ml::Window * wnd, ProjectParser* parser, RenderEngine* rnd) :
		m_parser(parser), m_renderer(rnd)
	{
		m_wnd = wnd;
		m_rts.clear();
		m_binds.clear();
		m_imgs.clear();
	}
	ObjectManager::~ObjectManager()
	{
		Clear();
	}
	void ObjectManager::Clear()
	{
		for (auto str : m_items) {
			if (!IsRenderTexture(str) && !IsAudio(str)) {
				delete m_texs[str];
				delete m_imgs[str];
			}
			else if (IsAudio(str)) {
				if (m_audioPlayer[str]->IsPlaying())
					m_audioPlayer[str]->Stop();

				delete m_audioData[str];
				delete m_audioPlayer[str];
				delete m_texs[str];
			}
			else
				delete m_rts[str];
			delete m_srvs[str];
		}
		
		if (m_rts.size() > 0) m_rts.clear();
		if (m_imgs.size() > 0) m_imgs.clear();
		if (m_srvs.size() > 0) m_srvs.clear();
		if (m_texs.size() > 0) m_texs.clear();
		if (m_binds.size() > 0) m_binds.clear(); // crashes here when opening GLSL file (?) TODO 
		if (m_audioData.size() > 0) m_audioData.clear();
		if (m_audioPlayer.size() > 0) m_audioPlayer.clear();

		m_audioMute.clear();
		m_audioExclude.clear();
		m_items.clear();
		m_isCube.clear();
	}
	bool ObjectManager::CreateRenderTexture(const std::string & name)
	{
		if (name.size() == 0 || m_rts.count(name) > 0 || name == "Window")
			return false;

		m_items.push_back(name);

		m_srvs[name] = new ml::ShaderResourceView();
		m_rts[name] = new ed::RenderTextureObject();

		m_rts[name]->RT = new ml::RenderTexture();
		m_rts[name]->RT->Create(*m_wnd, m_wnd->GetSize(), ml::Resource::ShaderResource, true, 32);
		m_srvs[name]->Create(*m_wnd, *m_rts[name]->RT);

		m_rts[name]->FixedSize = m_renderer->GetLastRenderSize();
		m_rts[name]->ClearColor = ml::Color(0, 0, 0, 0);
		m_rts[name]->Name = name;

		return true;
	}
	void ObjectManager::CreateTexture(const std::string& file, bool cube)
	{
		if (m_imgs.count(file) > 0)
			return;

		ml::Image* img = (m_imgs[file] = new ml::Image());
		ml::Texture* tex = (m_texs[file] = new ml::Texture());
		ml::ShaderResourceView* srv = (m_srvs[file] = new ml::ShaderResourceView());

		m_items.push_back(file);

		m_isCube[file] = cube;

		size_t imgDataLen = 0;
		char* imgData = m_parser->LoadProjectFile(file, imgDataLen);
		if (img->LoadFromMemory(imgData, imgDataLen, mGetImgType(file))) {
			tex->Create(*m_wnd, *img, ml::Resource::ShaderResource | (ml::Resource::TextureCube * cube));
			srv->Create(*m_wnd, *tex);
		}
		free(imgData);
	}
	bool ObjectManager::CreateAudio(const std::string& file)
	{
		if (!m_audioEngine.IsInitialized())
			return false;

		m_items.push_back(file);

		m_audioData[file] = new ml::AudioFile();
		m_audioData[file]->Load(m_parser->GetProjectPath(file), m_audioEngine);
		m_audioPlayer[file] = new ml::AudioPlayer();
		m_audioExclude[file] = 0;
		m_audioMute[file] = false;

		m_texs[file] = new ml::Texture();
		m_texs[file]->Create(*m_wnd, 512, 2, ml::Resource::ShaderResource | ml::Resource::Usage::Default, DXGI_FORMAT_R32_FLOAT);
		
		m_srvs[file] = new ml::ShaderResourceView();
		m_srvs[file]->Create(*m_wnd, *m_texs[file]);

		return true;
	}
	void ObjectManager::Update(float delta)
	{
		for (auto& it : m_audioData) {
			if (!it.second->IsLoading() && !m_audioPlayer[it.first]->IsPlaying()) {
				it.second->Finalize();
				if (!it.second->HasFailed())
					m_audioPlayer[it.first]->Play(*it.second, true);
			} else if (!it.second->IsLoading() && !it.second->HasFailed()) {
				// get samples and fft data
				ml::AudioPlayer* player = m_audioPlayer[it.first];
				int channels = it.second->GetInfo()->nChannels;
				int perChannel = it.second->GetTotalSamplesPerChannel();
				int curSample = player->SamplesPlayedCount() - m_audioExclude[it.first];

				if (curSample >= perChannel - 1) {
					m_audioExclude[it.first] += perChannel;
					curSample -= perChannel;
				}

				double* fftData = m_audioAnalyzer.FFT(*it.second, curSample);
				
				for (int i = 0; i < ed::AudioAnalyzer::SampleCount; i++) {
					m_audioTempTexData[i] = fftData[i/2];
					
					int16_t s = it.second->GetSample(std::min<int>(i + curSample, perChannel), false);
					float sf = (float)s / (float)INT16_MAX;
					m_audioTempTexData[i + ed::AudioAnalyzer::SampleCount] = sf * 0.5f + 0.5f;
				}

				m_texs[it.first]->Update(m_audioTempTexData, ed::AudioAnalyzer::SampleCount * sizeof(float));
			}
		}
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
		for (auto& i : m_binds)
			for (int j = 0; j < i.second.size(); j++)
				if (i.second[j] == m_srvs[file]) {
					i.second.erase(i.second.begin() + j);
					j--;
				}

		delete m_srvs[file];

		if (!IsRenderTexture(file) && !IsAudio(file)) {
			delete m_texs[file];
			delete m_imgs[file];
		}
		else if (IsAudio(file)) {
			if (m_audioPlayer[file]->IsPlaying())
				m_audioPlayer[file]->Stop();

			delete m_audioData[file];
			delete m_audioPlayer[file];
			delete m_texs[file];
			m_audioData.erase(file);
			m_audioPlayer.erase(file);
			m_audioExclude.erase(file);
			m_audioMute.erase(file);
		}
		else {
			delete m_rts[file];
			m_rts.erase(file);
		}


		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == file) {
				m_items.erase(m_items.begin() + i);
				break;
			}

		m_srvs.erase(file);
		m_texs.erase(file);
		m_imgs.erase(file);
		m_isCube.erase(file);
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
	DirectX::XMINT2 ObjectManager::GetRenderTextureSize(const std::string & name)
	{
		if (m_rts[name]->FixedSize.x < 0) return DirectX::XMINT2(m_rts[name]->RatioSize.x * m_renderer->GetLastRenderSize().x, m_rts[name]->RatioSize.y * m_renderer->GetLastRenderSize().y);
		return m_rts[name]->FixedSize;
	}
	void ObjectManager::Mute(const std::string& name)
	{
		m_audioPlayer[name]->SetVolume(0);
		m_audioMute[name] = true;
	}
	void ObjectManager::Unmute(const std::string& name)
	{
		m_audioPlayer[name]->SetVolume(1);
		m_audioMute[name] = false;
	}
	void ObjectManager::ResizeRenderTexture(const std::string & name, DirectX::XMINT2 size)
	{
		m_rts[name]->RT->Create(*m_wnd, size, ml::Resource::ShaderResource, true, 32);
		m_srvs[name]->Create(*m_wnd, *m_rts[name]->RT);
	}
}