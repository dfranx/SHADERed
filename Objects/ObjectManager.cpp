#include "ObjectManager.h"
#include "RenderEngine.h"
#include "Settings.h"
#include "Logger.h"
#include "../Engine/GLUtils.h"

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace ed
{
	ObjectManager::ObjectManager(ProjectParser* parser, RenderEngine* rnd) :
		m_parser(parser), m_renderer(rnd)
	{
		m_binds.clear();
	}
	ObjectManager::~ObjectManager()
	{
		Clear();
	}

	void loadCubemapFace(GLuint face, const std::string& path, int& w, int& h)
	{
		int nrChannels = 0;
		unsigned char* data = stbi_load(path.c_str(), &w, &h, &nrChannels, 0);
		unsigned char* paddedData = nullptr;

		if (nrChannels != 4) {
			paddedData = (unsigned char*)malloc(w * h * 4);
			for (int x = 0; x < w; x++) {
				for (int y = 0; y < h; y++) {
					if (nrChannels == 3) {
						paddedData[(y * w + x) * 4 + 0] = data[(y * w + x) * 3 + 0];
						paddedData[(y * w + x) * 4 + 1] = data[(y * w + x) * 3 + 1];
						paddedData[(y * w + x) * 4 + 2] = data[(y * w + x) * 3 + 2];
					} else if (nrChannels == 1) {
						unsigned char val = data[(y * w + x) * 1 + 0];
						paddedData[(y * w + x) * 4 + 0] = val;
						paddedData[(y * w + x) * 4 + 1] = val;
						paddedData[(y * w + x) * 4 + 2] = val;
					}
					paddedData[(y * w + x) * 4 + 3] = 255;
				}
			}
		}

		glTexImage2D(
			face,
			0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, paddedData == nullptr ? data : paddedData
		);

		if (paddedData != nullptr)
			free(paddedData);
		stbi_image_free(data);
	}

	void ObjectManager::Clear()
	{
		Logger::Get().Log("Clearing ObjectManager contents...");
		
		for (int i = 0; i < m_itemData.size(); i++) {
			if (m_itemData[i]->Plugin != nullptr) {
				PluginObject* pobj = m_itemData[i]->Plugin;
				pobj->Owner->RemoveObject(m_items[i].c_str(), pobj->Type, pobj->Data, pobj->ID);
			}
			
			delete m_itemData[i];
		}
		
		m_binds.clear();
		m_uniformBinds.clear();
		m_items.clear();
		m_itemData.clear();
	}
	bool ObjectManager::CreateRenderTexture(const std::string & name)
	{
		Logger::Get().Log("Creating a render texture " + name + " ...");

		if (Exists(name)) {
			Logger::Get().Log("Cannot create a render texture " + name + " because a rt with such name already exists", true);
			return false;
		}

		m_parser->ModifyProject();

		ObjectManagerItem* item = new ObjectManagerItem();
		m_itemData.push_back(item);
		m_items.push_back(name);

		ed::RenderTextureObject* rtObj = item->RT = new ed::RenderTextureObject();
		glm::ivec2 size = m_renderer->GetLastRenderSize();

		rtObj->FixedSize = size;
		rtObj->RatioSize = glm::vec2(-1, -1);
		rtObj->ClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		rtObj->Name = name;
		rtObj->Clear = true;
		rtObj->Format = GL_RGBA;

		// color texture
		glGenTextures(1, &item->Texture);
		glBindTexture(GL_TEXTURE_2D, item->Texture);
		glTexImage2D(GL_TEXTURE_2D, 0, rtObj->Format, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		// depth texture
		glGenTextures(1, &rtObj->DepthStencilBuffer);
		glBindTexture(GL_TEXTURE_2D, rtObj->DepthStencilBuffer);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, size.x, size.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);

		// color texture ms
		glGenTextures(1, &rtObj->BufferMS);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, rtObj->BufferMS);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, Settings::Instance().Preview.MSAA, rtObj->Format, size.x, size.y, true);

		// depth texture ms
		glGenTextures(1, &rtObj->DepthStencilBufferMS);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, rtObj->DepthStencilBufferMS);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, Settings::Instance().Preview.MSAA, GL_DEPTH24_STENCIL8, size.x, size.y, true);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

		return true;
	}
	bool ObjectManager::CreateTexture(const std::string& file)
	{
		Logger::Get().Log("Creating a texture " + file + " ...");

		if (Exists(file)) {
			Logger::Get().Log("Cannot create a texture " + file + " because that texture is already added to the project", true);
			return false;
		}

		std::string path = m_parser->GetProjectPath(file);
		int width, height, nrChannels;
		unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
		unsigned char* paddedData = nullptr;

		if (data == nullptr)
			return false;

		m_parser->ModifyProject();

		ObjectManagerItem* item = new ObjectManagerItem();
		m_itemData.push_back(item);
		m_items.push_back(file);

		item->IsTexture = true;

		if (data == nullptr)
			Logger::Get().Log("Failed to load a texture " + file + " from file", true);

		GLenum fmt = GL_RGB;
		if (nrChannels == 4)
			fmt = GL_RGBA;
		else if (nrChannels == 1)
			fmt = GL_LUMINANCE;

		if (nrChannels != 4) {
			paddedData = (unsigned char*)malloc(width * height * 4);
			for (int x = 0; x < width; x++) {
				for (int y = 0; y < height; y++) {
					if (nrChannels == 3) {
						paddedData[(y * width + x) * 4 + 0] = data[(y * width + x) * 3 + 0];
						paddedData[(y * width + x) * 4 + 1] = data[(y * width + x) * 3 + 1];
						paddedData[(y * width + x) * 4 + 2] = data[(y * width + x) * 3 + 2];
					} else if (nrChannels == 1) {
						unsigned char val = data[(y * width + x) * 1 + 0];
						paddedData[(y * width + x) * 4 + 0] = val;
						paddedData[(y * width + x) * 4 + 1] = val;
						paddedData[(y * width + x) * 4 + 2] = val;
					}
					paddedData[(y * width + x) * 4 + 3] = 255;
				}
			}
		}

		// normal texture
		glGenTextures(1, &item->Texture);
		glBindTexture(GL_TEXTURE_2D, item->Texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D,0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, paddedData == nullptr ? data : paddedData);
		glBindTexture(GL_TEXTURE_2D, 0);



		// flipped texture
		unsigned char* flippedData = (unsigned char*)malloc(width * height * 4);

		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				if (nrChannels == 4) { // use data
					flippedData[(y * width + x) * 4 + 0] = data[((height - y - 1) * width + x) * 4 + 0];
					flippedData[(y * width + x) * 4 + 1] = data[((height - y - 1) * width + x) * 4 + 1];
					flippedData[(y * width + x) * 4 + 2] = data[((height - y - 1) * width + x) * 4 + 2];
					flippedData[(y * width + x) * 4 + 3] = data[((height - y - 1) * width + x) * 4 + 3];
				} else { // use paddedData
					flippedData[(y * width + x) * 4 + 0] = paddedData[((height - y - 1) * width + x) * 4 + 0];
					flippedData[(y * width + x) * 4 + 1] = paddedData[((height - y - 1) * width + x) * 4 + 1];
					flippedData[(y * width + x) * 4 + 2] = paddedData[((height - y - 1) * width + x) * 4 + 2];
					flippedData[(y * width + x) * 4 + 3] = paddedData[((height - y - 1) * width + x) * 4 + 3];
				}
			}
		}

		glGenTextures(1, &item->FlippedTexture);
		glBindTexture(GL_TEXTURE_2D, item->FlippedTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, flippedData);
		glBindTexture(GL_TEXTURE_2D, 0);



		item->ImageSize = glm::ivec2(width, height);

		free(flippedData);
		if (paddedData != nullptr)
			free(paddedData);

		stbi_image_free(data);

		return true;
	}
	bool ObjectManager::CreateCubemap(const std::string& name, const std::string& left, const std::string& top, const std::string& front, const std::string& bottom, const std::string& right, const std::string& back)
	{
		Logger::Get().Log("Creating a cubemap " + name + " ...");

		if (Exists(name)) {
			Logger::Get().Log("Cannot create a cubemap " + name + " because cubemap with such name already exists in the project", true);
			return false;
		}

		m_parser->ModifyProject();

		ObjectManagerItem* item = new ObjectManagerItem();
		m_itemData.push_back(item);
		m_items.push_back(name);

		item->IsCube = true;

		glGenTextures(1, &item->Texture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, item->Texture);
		int width, height;

		// properties
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		
		// left face
		loadCubemapFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, m_parser->GetProjectPath(left), width, height);
		item->CubemapPaths.push_back(left);

		// top
		loadCubemapFace(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, m_parser->GetProjectPath(top), width, height);
		item->CubemapPaths.push_back(top);

		// front
		loadCubemapFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, m_parser->GetProjectPath(front), width, height);
		item->CubemapPaths.push_back(front);

		// bottom
		loadCubemapFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, m_parser->GetProjectPath(bottom), width, height);
		item->CubemapPaths.push_back(bottom);

		// right
		loadCubemapFace(GL_TEXTURE_CUBE_MAP_POSITIVE_X, m_parser->GetProjectPath(right), width, height);
		item->CubemapPaths.push_back(right);

		// back
		loadCubemapFace(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, m_parser->GetProjectPath(back), width, height);
		item->CubemapPaths.push_back(back);
		
		// clean up
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		item->ImageSize = glm::ivec2(width, height);

		return true;
	}
	bool ObjectManager::CreateAudio(const std::string& file)
	{
		Logger::Get().Log("Creating audio object from file " + file + " ...");

		if (Exists(file)) {
			Logger::Get().Log("Audio object " + file + " already exists in the project", true);
			return false;
		}

		ObjectManagerItem* item = new ObjectManagerItem();

		item->SoundBuffer = new sf::SoundBuffer();
		bool loaded = item->SoundBuffer->loadFromFile(m_parser->GetProjectPath(file));
		if (!loaded) {
			delete item;
			ed::Logger::Get().Log("Failed to load an audio file " + file, true);
			return false;
		}

		m_itemData.push_back(item);
		m_parser->ModifyProject();
		m_items.push_back(file);

		glGenTextures(1, &item->Texture);
		glBindTexture(GL_TEXTURE_2D, item->Texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 512, 2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		item->Sound = new sf::Sound();
		item->Sound->setBuffer(*(item->SoundBuffer));
		item->Sound->setLoop(true);
		item->Sound->play();
		item->SoundMuted = false;

		return true;
	}
	bool ObjectManager::CreateBuffer(const std::string& name)
	{
		Logger::Get().Log("Creating a buffer " + name + " ...");

		if (Exists(name)) {
			Logger::Get().Log("Cannot create the buffer " + name + " because an item with such name already exists", true);
			return false;
		}

		m_parser->ModifyProject();

		ObjectManagerItem* item = new ObjectManagerItem();
		m_itemData.push_back(item);
		m_items.push_back(name);

		ed::BufferObject* bObj = item->Buffer = new ed::BufferObject();
		glm::ivec2 size = m_renderer->GetLastRenderSize();

		bObj->Size = 0;
		bObj->Data = nullptr;
		strcpy(bObj->ViewFormat, "float");

		glGenBuffers(1, &bObj->ID);
		glBindBuffer(GL_UNIFORM_BUFFER, bObj->ID);
		glBufferData(GL_UNIFORM_BUFFER, 0, NULL, GL_STATIC_DRAW); // allocate 0 bytes of memory
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		return true;
	}
	bool ObjectManager::CreateImage(const std::string& name, glm::ivec2 size)
	{
		Logger::Get().Log("Creating an image " + name + " ...");

		if (Exists(name)) {
			Logger::Get().Log("Cannot create the image " + name + " because an item with exact name already exists", true);
			return false;
		}

		m_parser->ModifyProject();

		ObjectManagerItem* item = new ObjectManagerItem();
		m_itemData.push_back(item);
		m_items.push_back(name);

		ed::ImageObject* iObj = item->Image = new ImageObject();

		glGenTextures(1, &iObj->Texture);
		glBindTexture(GL_TEXTURE_2D, iObj->Texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		iObj->Size = size;
		iObj->Format = GL_RGBA32F;

		return true;
	}
	bool ObjectManager::CreateImage3D(const std::string& name, glm::ivec3 size)
	{
		Logger::Get().Log("Creating an image " + name + " ...");

		if (Exists(name)) {
			Logger::Get().Log("Cannot create the image " + name + " because an item with exact name already exists", true);
			return false;
		}

		m_parser->ModifyProject();

		ObjectManagerItem* item = new ObjectManagerItem();
		m_itemData.push_back(item);
		m_items.push_back(name);

		ed::Image3DObject* iObj = item->Image3D = new Image3DObject();
		iObj->Size = size;
		iObj->Format = GL_RGBA32F;

		glGenTextures(1, &iObj->Texture);
		glBindTexture(GL_TEXTURE_3D, iObj->Texture);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage3D(GL_TEXTURE_3D, 0, iObj->Format, size.x, size.y, size.z, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_3D, 0);

		return true;
	}
	bool ObjectManager::CreatePluginItem(const std::string& name, const std::string& objtype, void* data, GLuint id, IPlugin* owner)
	{
		Logger::Get().Log("Creating a plugin object " + name + " of type " + objtype + "...");

		if (Exists(name)) {
			Logger::Get().Log("Cannot create the plugin object " + name + " because an item with that name already exists", true);
			return false;
		}

		m_parser->ModifyProject();

		ObjectManagerItem* item = new ObjectManagerItem();
		m_itemData.push_back(item);
		m_items.push_back(name);

		PluginObject* pObj = item->Plugin = new PluginObject();
		strcpy(pObj->Type, objtype.c_str());
		pObj->Owner = owner;
		pObj->Data = data;
		pObj->ID = id;
		
		return true;
	}

	ShaderVariable::ValueType getValueType(const std::string& name)
	{
		if (name == "bool")
			return ShaderVariable::ValueType::Boolean1;
		else if (name == "bvec2" || name == "bool2")
			return ShaderVariable::ValueType::Boolean2;
		else if (name == "bvec3" || name == "bool3")
			return ShaderVariable::ValueType::Boolean3;
		else if (name == "bvec4" || name == "bool4")
			return ShaderVariable::ValueType::Boolean4;
		else if (name == "int")
			return ShaderVariable::ValueType::Integer1;
		else if (name == "ivec2" || name == "int2")
			return ShaderVariable::ValueType::Integer2;
		else if (name == "ivec3" || name == "int3")
			return ShaderVariable::ValueType::Integer3;
		else if (name == "ivec4" || name == "int4")
			return ShaderVariable::ValueType::Integer4;
		else if (name == "float")
			return ShaderVariable::ValueType::Float1;
		else if (name == "vec2" || name == "float2")
			return ShaderVariable::ValueType::Float2;
		else if (name == "vec3" || name == "float3")
			return ShaderVariable::ValueType::Float3;
		else if (name == "vec4" || name == "float4")
			return ShaderVariable::ValueType::Float4;
		else if (name == "mat2" || name == "float2x2")
			return ShaderVariable::ValueType::Float2x2;
		else if (name == "mat3" || name == "float3x3")
			return ShaderVariable::ValueType::Float3x3;
		else if (name == "mat4" || name == "float4x4")
			return ShaderVariable::ValueType::Float4x4;

		return ShaderVariable::ValueType::Float1;
	}
	std::vector<ShaderVariable::ValueType> ObjectManager::ParseBufferFormat(const std::string& str)
	{
		std::vector<ed::ShaderVariable::ValueType> ret;
		std::string tok = str;
		size_t pos = tok.find_first_of(';');
		while (pos != std::string::npos)
		{
			std::string tokpart = tok.substr(0, pos);
			if (tokpart.size() > 0)
				ret.push_back(getValueType(tokpart));

			if (pos+1 < tok.size())
				tok = tok.substr(pos+1);
			else {
				tok = tok.substr(pos);
				break;
			}
			pos = tok.find_first_of(';');
		}

		if (tok.size() > 2)
			ret.push_back(getValueType(tok));

		return ret;
	}
	
	void ObjectManager::Update(float delta)
	{
		for (auto& it : m_itemData) {
			if (it->SoundBuffer == nullptr)
				continue;

			// get samples and fft data
			sf::Sound* player = it->Sound;
			int channels = it->SoundBuffer->getChannelCount();
			int perChannel = it->SoundBuffer->getSampleCount() / channels;
			int curSample = (int)((player->getPlayingOffset().asSeconds() / it->SoundBuffer->getDuration().asSeconds()) * perChannel);

			double* fftData = m_audioAnalyzer.FFT(*(it->SoundBuffer), curSample);

			const sf::Int16* samples = it->SoundBuffer->getSamples();
			for (int i = 0; i < ed::AudioAnalyzer::SampleCount; i++) {
				sf::Int16 s = samples[std::min<int>(i + curSample, perChannel)];
				float sf = (float)s / (float)INT16_MAX;

				m_audioTempTexData[i] = fftData[i / 2];
				m_audioTempTexData[i + ed::AudioAnalyzer::SampleCount] = sf* 0.5f + 0.5f;
			}

			glBindTexture(GL_TEXTURE_2D, it->Texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 512, 2, 0, GL_RED, GL_FLOAT, m_audioTempTexData);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}
	void ObjectManager::Remove(const std::string & file)
	{
		m_parser->ModifyProject();

		GLuint srv = GetTexture(file);
		if (IsImage3D(file))
			srv = GetImage3D(file)->Texture;
		else if (IsImage(file))
			srv = GetImage(file)->Texture;
		else if (IsBuffer(file))
			srv = GetBuffer(file)->ID;
		else if (IsPluginObject(file))
			srv = GetPluginObject(file)->ID;

		for (auto& i : m_binds)
			for (int j = 0; j < i.second.size(); j++)
				if (i.second[j] == srv) {
					i.second.erase(i.second.begin() + j);
					j--;
				}
		for (auto& i : m_uniformBinds)
			for (int j = 0; j < i.second.size(); j++)
				if (i.second[j] == srv) {
					i.second.erase(i.second.begin() + j);
					j--;
				}
		
		int index = 0;
		for (; index < m_items.size(); index++)
			if (m_items[index] == file) break;

		if (IsPluginObject(file)) {
			PluginObject* pobj = GetPluginObject(file);
			pobj->Owner->RemoveObject(file.c_str(), pobj->Type, pobj->Data, pobj->ID);
		}

		delete m_itemData[index];
		m_itemData.erase(m_itemData.begin() + index);
		m_items.erase(m_items.begin() + index);
	}

	void ObjectManager::Bind(const std::string & file, PipelineItem * pass)
	{
		if (IsBound(file, pass) == -1) {
			m_parser->ModifyProject();

			if (IsImage(file))
				m_binds[pass].push_back(GetImage(file)->Texture);
			else if (IsImage3D(file))
				m_binds[pass].push_back(GetImage3D(file)->Texture);
			else if (IsPluginObject(file))
				m_binds[pass].push_back(GetPluginObject(file)->ID);
			else
				m_binds[pass].push_back(GetTexture(file));
		}
	}
	void ObjectManager::Unbind(const std::string & file, PipelineItem * pass)
	{
		std::vector<GLuint>& srvs = m_binds[pass];

		GLuint srv = GetTexture(file);
		if (IsImage(file))
			srv = GetImage(file)->Texture;
		else if (IsImage3D(file))
			srv = GetImage3D(file)->Texture;
		else if (IsPluginObject(file))
			srv = GetPluginObject(file)->ID;

		for (int i = 0; i < srvs.size(); i++)
			if (srvs[i] == srv) {
				m_parser->ModifyProject();

				srvs.erase(srvs.begin() + i);
				return;
			}
	}
	int ObjectManager::IsBound(const std::string & file, PipelineItem * pass)
	{
		if (m_binds.count(pass) == 0)
			return -1;

		if (IsImage(file)) {
			for (int i = 0; i < m_binds[pass].size(); i++)
				if (m_binds[pass][i] == GetImage(file)->Texture)
					return i;
		} else if (IsImage3D(file)) {
			for (int i = 0; i < m_binds[pass].size(); i++)
				if (m_binds[pass][i] == GetImage3D(file)->Texture)
					return i;
		} else if (IsPluginObject(file)) {
			for (int i = 0; i < m_binds[pass].size(); i++)
				if (m_binds[pass][i] == GetPluginObject(file)->ID)
					return i;
		} else {
			for (int i = 0; i < m_binds[pass].size(); i++)
				if (m_binds[pass][i] == GetTexture(file))
					return i;
		}

		return -1;
	}

	void ObjectManager::BindUniform(const std::string & file, PipelineItem * pass)
	{
		if (IsUniformBound(file, pass) == -1) {
			if (IsBuffer(file)) 
				m_uniformBinds[pass].push_back(GetBuffer(file)->ID);
			else if (IsImage3D(file))
				m_uniformBinds[pass].push_back(GetImage3D(file)->Texture);
			else if (IsPluginObject(file))
				m_uniformBinds[pass].push_back(GetPluginObject(file)->ID);
			else
				m_uniformBinds[pass].push_back(GetImage(file)->Texture);

			m_parser->ModifyProject();
		}
	}
	void ObjectManager::UnbindUniform(const std::string & file, PipelineItem * pass)
	{
		std::vector<GLuint>& ubos = m_uniformBinds[pass];
		GLuint itemID = 0;

		if (IsBuffer(file)) 
			itemID = GetBuffer(file)->ID;
		else if (IsImage3D(file))
			itemID = GetImage3D(file)->Texture;
		else if (IsPluginObject(file))
			itemID = GetPluginObject(file)->ID;
		else
			itemID = GetImage(file)->Texture;
		
		for (int i = 0; i < ubos.size(); i++)
			if (ubos[i] == itemID) {
				ubos.erase(ubos.begin() + i);
				m_parser->ModifyProject();
				return;
			}
	}
	int ObjectManager::IsUniformBound(const std::string & file, PipelineItem * pass)
	{
		if (m_uniformBinds.count(pass) == 0)
			return -1;

		GLuint itemID = 0;
		if (IsBuffer(file)) 
			itemID = GetBuffer(file)->ID;
		else if (IsImage3D(file))
			itemID = GetImage3D(file)->Texture;
		else if (IsPluginObject(file))
			itemID = GetPluginObject(file)->ID;
		else
			itemID = GetImage(file)->Texture;

		for (int i = 0; i < m_uniformBinds[pass].size(); i++)
			if (m_uniformBinds[pass][i] == itemID)
				return i;

		return -1;
	}

	std::string ObjectManager::GetItemNameByTextureID(GLuint texID)
	{
		for (int i = 0; i < m_itemData.size(); i++) {
			ObjectManagerItem* item = m_itemData[i];
			if (item->Texture == texID ||
				(item->Image != nullptr && item->Image->Texture == texID) ||
				(item->Image3D != nullptr && item->Image3D->Texture == texID) ||
				(item->Buffer != nullptr && item->Buffer->ID == texID))
			{
				return m_items[i];
			}
		}

		return "";
	}
	glm::ivec2 ObjectManager::GetRenderTextureSize(const std::string & name)
	{
		RenderTextureObject* rt = GetRenderTexture(name);
		if (rt->FixedSize.x < 0) return glm::ivec2(rt->RatioSize.x * m_renderer->GetLastRenderSize().x, rt->RatioSize.y * m_renderer->GetLastRenderSize().y);
		return rt->FixedSize;
	}
	const std::vector<std::string>& ObjectManager::GetCubemapTextures(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->CubemapPaths;
		return m_emptyCBTexs;
	}

	bool ObjectManager::IsRenderTexture(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->RT != nullptr;
		return false;
	}
	bool ObjectManager::IsCubeMap(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->IsCube;
		return false;
	}
	bool ObjectManager::IsAudio(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Sound != nullptr;
		return false;
	}
	bool ObjectManager::IsAudioMuted(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->SoundMuted;
		return false;
	}
	bool ObjectManager::IsBuffer(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Buffer != nullptr;
		return false;
	}
	bool ObjectManager::IsImage(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Image != nullptr;
		return false;
	}
	bool ObjectManager::IsImage3D(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Image3D != nullptr;
		return false;
	}
	bool ObjectManager::IsPluginObject(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Plugin != nullptr;
		return false;
	}
	bool ObjectManager::IsPluginObject(GLuint id)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_itemData[i]->Plugin != nullptr && m_itemData[i]->Plugin->ID == id)
				return true;
		return false;
	}
	bool ObjectManager::IsCubeMap(GLuint id)
	{
		for (const auto& i : m_itemData)
			if (i->Texture == id)
				return i->IsCube;
		return false;
	}
	bool ObjectManager::IsImage(GLuint id)
	{
		for (const auto& i : m_itemData)
			if (i->Image != nullptr && i->Image->Texture == id)
				return true;
		return false;
	}
	bool ObjectManager::IsImage3D(GLuint id)
	{
		for (const auto& i : m_itemData)
			if (i->Image3D != nullptr && i->Image3D->Texture == id)
				return true;
		return false;
	}

	GLuint ObjectManager::GetTexture(const std::string& file)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == file)
				return m_itemData[i]->Texture;
		return 0;
	}
	GLuint ObjectManager::GetFlippedTexture(const std::string& file)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == file)
				return m_itemData[i]->FlippedTexture;
		return 0;
	}
	glm::ivec2 ObjectManager::GetTextureSize(const std::string& file)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == file)
				return m_itemData[i]->ImageSize;
		return glm::ivec2(0,0);
	}
	sf::SoundBuffer* ObjectManager::GetSoundBuffer(const std::string& file)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == file)
				return m_itemData[i]->SoundBuffer;
		return nullptr;
	}
	sf::Sound* ObjectManager::GetAudioPlayer(const std::string& file)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == file)
				return m_itemData[i]->Sound;
		return nullptr;
	}
	BufferObject* ObjectManager::GetBuffer(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Buffer;
		return nullptr;
	}
	ImageObject* ObjectManager::GetImage(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Image;
		return nullptr;
	}
	Image3DObject* ObjectManager::GetImage3D(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Image3D;
		return nullptr;
	}
	glm::ivec2 ObjectManager::GetImageSize(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Image->Size;
		return glm::ivec2(0,0);
	}
	glm::ivec3 ObjectManager::GetImage3DSize(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Image3D->Size;
		return glm::ivec3(0, 0, 0);
	}
	RenderTextureObject* ObjectManager::GetRenderTexture(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->RT;
		return nullptr;
	}
	PluginObject* ObjectManager::GetPluginObject(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Plugin;
		return nullptr;
	}
	PluginObject* ObjectManager::GetPluginObject(GLuint id)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_itemData[i]->Plugin != nullptr && m_itemData[i]->Plugin->ID == id)
				return m_itemData[i]->Plugin;
		return nullptr;
	}

	RenderTextureObject* ObjectManager::GetRenderTexture(GLuint tex)
	{
		for (const auto& i : m_itemData)
			if (i->Texture == tex)
				return i->RT;
		return nullptr;
	}
	std::string ObjectManager::GetBufferNameByID(int id)
	{
		for (int i = 0; i < m_itemData.size(); i++)
			if (m_itemData[i]->Buffer != nullptr && m_itemData[i]->Buffer->ID == id)
				return m_items[i];
		return "";
	}
	std::string ObjectManager::GetImageNameByID(GLuint id)
	{
		for (int i = 0; i < m_itemData.size(); i++)
			if (m_itemData[i]->Image != nullptr && m_itemData[i]->Image->Texture == id)
				return m_items[i];
		return "";
	}
	std::string ObjectManager::GetImage3DNameByID(GLuint id)
	{
		for (int i = 0; i < m_itemData.size(); i++)
			if (m_itemData[i]->Image3D != nullptr && m_itemData[i]->Image3D->Texture == id)
				return m_items[i];
		return "";
	}

	ObjectManagerItem* ObjectManager::GetObjectManagerItem(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i];
		return nullptr;
	}
	std::string ObjectManager::GetObjectManagerItemName(ObjectManagerItem* item)
	{
		for (int i = 0; i < m_itemData.size(); i++)
			if (m_itemData[i] == item)
				return m_items[i];
		return "";
	}

	void ObjectManager::Mute(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (m_items[i] == name) {
				m_itemData[i]->SoundMuted = true;
				m_itemData[i]->Sound->setVolume(0);
				break;
			}
		}
	}
	void ObjectManager::Unmute(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (m_items[i] == name) {
				m_itemData[i]->SoundMuted = false;
				m_itemData[i]->Sound->setVolume(100);
				break;
			}
		}
	}

	void ObjectManager::ResizeRenderTexture(const std::string & name, glm::ivec2 size)
	{
		RenderTextureObject* rtObj = GetRenderTexture(name);

		if (rtObj->RatioSize.x == -1 && rtObj->RatioSize.y == -1)
			m_parser->ModifyProject();

		glBindTexture(GL_TEXTURE_2D, GetTexture(name));
		glTexImage2D(GL_TEXTURE_2D, 0, rtObj->Format, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_2D, rtObj->DepthStencilBuffer);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, size.x, size.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, rtObj->BufferMS);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, Settings::Instance().Preview.MSAA, rtObj->Format, size.x, size.y, true);

		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, rtObj->DepthStencilBufferMS);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, Settings::Instance().Preview.MSAA, GL_DEPTH24_STENCIL8, size.x, size.y, true);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
	}
	void ObjectManager::ResizeImage(const std::string& name, glm::ivec2 size)
	{
		ImageObject* iobj = GetImage(name);

		m_parser->ModifyProject();

		iobj->Size = size;

		glBindTexture(GL_TEXTURE_2D, iobj->Texture);
		glTexImage2D(GL_TEXTURE_2D, 0, iobj->Format, iobj->Size.x, iobj->Size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	void ObjectManager::ResizeImage3D(const std::string& name, glm::ivec3 size)
	{
		Image3DObject* iobj = GetImage3D(name);

		m_parser->ModifyProject();

		iobj->Size = size;

		glBindTexture(GL_TEXTURE_3D, iobj->Texture);
		glTexImage3D(GL_TEXTURE_3D, 0, iobj->Format, iobj->Size.x, iobj->Size.y, iobj->Size.z, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_3D, 0);
	}
}