#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/ObjectManager.h>
#include <SHADERed/Objects/RenderEngine.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Engine/Model.h>

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

#include <unordered_map>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

namespace ed {
	ObjectManager::ObjectManager(ProjectParser* parser, RenderEngine* rnd)
			: m_parser(parser)
			, m_renderer(rnd)
	{
		m_binds.clear();
		memset(m_kbTexture, 0, sizeof(unsigned char) * 256 * 3);
	}
	ObjectManager::~ObjectManager()
	{
		Clear();
	}

	void loadCubemapFace(GLuint face, const std::string& path, int& w, int& h)
	{
		stbi_set_flip_vertically_on_load(0);

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
			0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, paddedData == nullptr ? data : paddedData);

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
				pobj->Owner->Object_Remove(m_items[i].c_str(), pobj->Type, pobj->Data, pobj->ID);
			}

			delete m_itemData[i];
		}

		m_binds.clear();
		m_uniformBinds.clear();
		m_items.clear();
		m_itemData.clear();
	}
	bool ObjectManager::CreateRenderTexture(const std::string& name)
	{
		Logger::Get().Log("Creating a render texture " + name + " ...");

		if (name.size() == 0 || Exists(name)) {
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

		stbi_set_flip_vertically_on_load(1);

		std::string path = m_parser->GetProjectPath(file);
		int width, height, nrChannels;
		unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);
		
		if (data == nullptr) {
			Logger::Get().Log("Failed to load a texture " + file + " from file", true);
			return false;
		}

		m_parser->ModifyProject();

		ObjectManagerItem* item = new ObjectManagerItem();
		m_itemData.push_back(item);
		m_items.push_back(file);

		item->IsTexture = true;

		// normal texture
		glGenTextures(1, &item->Texture);
		glBindTexture(GL_TEXTURE_2D, item->Texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, item->Texture_MinFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, item->Texture_MagFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, item->Texture_WrapS);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, item->Texture_WrapT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		// flipped texture
		unsigned char* flippedData = (unsigned char*)malloc(width * height * 4);

		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				flippedData[(y * width + x) * 4 + 0] = data[((height - y - 1) * width + x) * 4 + 0];
				flippedData[(y * width + x) * 4 + 1] = data[((height - y - 1) * width + x) * 4 + 1];
				flippedData[(y * width + x) * 4 + 2] = data[((height - y - 1) * width + x) * 4 + 2];
				flippedData[(y * width + x) * 4 + 3] = data[((height - y - 1) * width + x) * 4 + 3];
			}
		}

		glGenTextures(1, &item->FlippedTexture);
		glBindTexture(GL_TEXTURE_2D, item->FlippedTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, item->Texture_MinFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, item->Texture_MagFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, item->Texture_WrapS);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, item->Texture_WrapT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, flippedData);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		item->ImageSize = glm::ivec2(width, height);

		free(flippedData);
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

		if (name.size() == 0 || Exists(name)) {
			Logger::Get().Log("Cannot create the buffer " + name + " because an item with such name already exists", true);
			return false;
		}

		m_parser->ModifyProject();

		ObjectManagerItem* item = new ObjectManagerItem();
		m_itemData.push_back(item);
		m_items.push_back(name);

		ed::BufferObject* bObj = item->Buffer = new ed::BufferObject();
		glm::ivec2 size = m_renderer->GetLastRenderSize();

		bObj->PreviewPaused = false;
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

		if (name.size() == 0 || Exists(name)) {
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

		memset(iObj->DataPath, 0, sizeof(char) * SHADERED_MAX_PATH);
		iObj->Size = size;
		iObj->Format = GL_RGBA32F;

		return true;
	}
	bool ObjectManager::CreateImage3D(const std::string& name, glm::ivec3 size)
	{
		Logger::Get().Log("Creating an image " + name + " ...");

		if (name.size() == 0 || Exists(name)) {
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
	bool ObjectManager::CreatePluginItem(const std::string& name, const std::string& objtype, void* data, GLuint id, IPlugin1* owner)
	{
		Logger::Get().Log("Creating a plugin object " + name + " of type " + objtype + "...");

		if (name.size() == 0 || Exists(name)) {
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
	bool ObjectManager::CreateKeyboardTexture(const std::string& name)
	{
		Logger::Get().Log("Creating a keyboard texture " + name + " ...");

		if (name.size() == 0 || Exists(name)) {
			Logger::Get().Log("Cannot create a keyboard texture " + name + " because an object with that name already exists", true);
			return false;
		}

		m_parser->ModifyProject();

		ObjectManagerItem* item = new ObjectManagerItem();
		m_itemData.push_back(item);
		m_items.push_back(name);

		item->IsTexture = true;
		item->IsKeyboardTexture = true;

		GLenum fmt = GL_RED;
		int width = 256, height = 3;

		// normal texture
		glGenTextures(1, &item->Texture);
		glBindTexture(GL_TEXTURE_2D, item->Texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_kbTexture);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		item->ImageSize = glm::ivec2(width, height);

		return true;
	}
		
	ShaderVariable::ValueType getValueType(const std::string& arg)
	{
		size_t trimLeft = 0, trimRight = arg.size();
		for (; trimLeft < arg.size(); trimLeft++)
			if (!isspace(arg[trimLeft]))
				break;
		for (; trimRight > 0; trimRight--)
			if (!isspace(arg[trimRight]))
				break;

		std::string name = arg.substr(trimLeft, trimRight - trimLeft);

		if (name == "bool" || name == "byte")
			return ShaderVariable::ValueType::Boolean1;
		else if (name == "bvec2" || name == "bool2" || name == "byte2")
			return ShaderVariable::ValueType::Boolean2;
		else if (name == "bvec3" || name == "bool3" || name == "byte3")
			return ShaderVariable::ValueType::Boolean3;
		else if (name == "bvec4" || name == "bool4" || name == "byte4")
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

		size_t pos = 0, lastIndex = 0;
		for (; pos < str.size(); pos++) {
			if (str[pos] == ';') {
				ret.push_back(getValueType(str.substr(lastIndex, pos - lastIndex)));
				lastIndex = pos + 1;
			}
		}

		if (lastIndex < str.size())
			ret.push_back(getValueType(str.substr(lastIndex, str.size() - lastIndex)));

		return ret;
	}

	bool ObjectManager::LoadBufferFromTexture(BufferObject* buf, const std::string& str, bool convertToFloat)
	{
		std::string path = m_parser->GetProjectPath(str);
		int width, height, nrChannels;
		unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
		
		if (data != nullptr) {
			m_parser->ModifyProject();

			if (convertToFloat) {
				buf->Size = width * height * nrChannels * sizeof(float);
				buf->Data = realloc(buf->Data, buf->Size);
				float* fData = (float*)buf->Data;

				for (int x = 0; x < width; x++) {
					for (int y = 0; y < height; y++) {
						for (int z = 0; z < nrChannels; z++) {
							int index = ((y * width) + x) * nrChannels + z;
							fData[index] = data[index] / 255.0f;
						}
					}
				}
			} else {
				buf->Size = width * height * nrChannels * sizeof(char);
				buf->Data = realloc(buf->Data, buf->Size);
				memcpy(buf->Data, data, buf->Size);
			}

			stbi_image_free(data);

			
			glBindBuffer(GL_UNIFORM_BUFFER, buf->ID);
			glBufferData(GL_UNIFORM_BUFFER, buf->Size, buf->Data, GL_STATIC_DRAW); // upload data
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}

		return data != nullptr;
	}
	bool ObjectManager::LoadBufferFromModel(BufferObject* buf, const std::string& str)
	{
		ed::eng::Model mdl;
		bool ret = mdl.LoadFromFile(str);

		if (ret) {
			int vertCount = 0;
			for (auto mesh : mdl.Meshes)
				vertCount += mesh.Vertices.size();
			int bufSize = vertCount * 4 * sizeof(float);

			buf->Size = bufSize;
			buf->Data = realloc(buf->Data, bufSize);

			int index = 0;
			float* fData = (float*)buf->Data;
			for (auto mesh : mdl.Meshes)
				for (auto vert : mesh.Vertices) {
					fData[index + 0] = vert.Position.x;
					fData[index + 1] = vert.Position.y;
					fData[index + 2] = vert.Position.z;
					fData[index + 3] = 1.0f;
					index += 4;
				}

			glBindBuffer(GL_UNIFORM_BUFFER, buf->ID);
			glBufferData(GL_UNIFORM_BUFFER, buf->Size, buf->Data, GL_STATIC_DRAW); // upload data
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}

		return ret;
	}
	bool ObjectManager::LoadBufferFromFile(BufferObject* buf, const std::string& str)
	{
		std::string bPath = m_parser->GetProjectPath(str);
		std::ifstream bufRead(bPath, std::ios::binary | std::ios::ate);
		size_t bufSize = bufRead.tellg();
		bufRead.seekg(0, std::ios::beg);

		bool ret = bufRead.is_open();
		if (ret) {
			buf->Size = bufSize;
			buf->Data = realloc(buf->Data, bufSize);
			char* data = (char*)calloc(1, bufSize);
			bufRead.read(data, bufSize);
			memcpy(buf->Data, data, bufSize);
			free(data);

			glBindBuffer(GL_UNIFORM_BUFFER, buf->ID);
			glBufferData(GL_UNIFORM_BUFFER, buf->Size, buf->Data, GL_STATIC_DRAW); // upload data
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}

		bufRead.close();

		return ret;
	}

	bool ObjectManager::ReloadTexture(ObjectManagerItem* item, const std::string& newPath)
	{
		stbi_set_flip_vertically_on_load(1);

		for (int i = 0; i < m_itemData.size(); i++) {
			if (m_itemData[i] == item) {
				std::string path = m_parser->GetProjectPath(newPath);
				int width, height, nrChannels;
				unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);

				if (data == nullptr)
					return false;

				if (m_items[i] != newPath) {
					m_items[i] = newPath;
					m_parser->ModifyProject();
				}

				// normal texture
				glBindTexture(GL_TEXTURE_2D, item->Texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				glGenerateMipmap(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, 0);

				// flipped texture
				unsigned char* flippedData = (unsigned char*)malloc(width * height * 4);
				for (int x = 0; x < width; x++) {
					for (int y = 0; y < height; y++) {
						flippedData[(y * width + x) * 4 + 0] = data[((height - y - 1) * width + x) * 4 + 0];
						flippedData[(y * width + x) * 4 + 1] = data[((height - y - 1) * width + x) * 4 + 1];
						flippedData[(y * width + x) * 4 + 2] = data[((height - y - 1) * width + x) * 4 + 2];
						flippedData[(y * width + x) * 4 + 3] = data[((height - y - 1) * width + x) * 4 + 3];
					}
				}

				glGenTextures(1, &item->FlippedTexture);
				glBindTexture(GL_TEXTURE_2D, item->FlippedTexture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, flippedData);
				glGenerateMipmap(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, 0);

				item->ImageSize = glm::ivec2(width, height);

				free(flippedData);
				stbi_image_free(data);
				
				return true;
			}
		}

		return false;
	}

	void ObjectManager::Pause(bool pause)
	{
		for (auto& it : m_itemData) {
			if (it->SoundBuffer == nullptr)
				continue;

			// get samples and fft data
			sf::Sound* player = it->Sound;
			

			if (pause)
				player->pause();
			else
				player->play();
		}
	}
	void ObjectManager::OnEvent(const SDL_Event& e)
	{
		std::unordered_map<SDL_Keycode, int> keyIDs = {
			{ SDLK_BACKSPACE, 8 },
			{ SDLK_TAB, 9 },
			{ SDLK_RETURN, 13 },
			// { SHIFT, 16 },
			// { CTRL, 17 },
			// { ALT, 18 },
			{ SDLK_PAUSE, 19 },
			{ SDLK_CAPSLOCK, 20 }, 
			{ SDLK_ESCAPE, 27 },
			{ SDLK_PAGEUP, 33 },
			{ SDLK_SPACE, 32 },
			{ SDLK_PAGEDOWN, 34 },
			{ SDLK_END, 35 },
			{ SDLK_HOME, 36 },
			{ SDLK_LEFT, 37 },
			{ SDLK_UP, 38 },
			{ SDLK_RIGHT, 39 },
			{ SDLK_DOWN, 40 },
			{ SDLK_PRINTSCREEN, 44 }, 
			{ SDLK_INSERT, 45 },
			{ SDLK_DELETE, 46 },
			{ SDLK_0, 48 },
			{ SDLK_1, 49 },
			{ SDLK_2, 50 },
			{ SDLK_3, 51 },
			{ SDLK_4, 52 },
			{ SDLK_5, 53 },
			{ SDLK_6, 54 },
			{ SDLK_7, 55 },
			{ SDLK_8, 56 },
			{ SDLK_9, 57 },
			{ SDLK_a, 65 },
			{ SDLK_b, 66 },
			{ SDLK_c, 67 },
			{ SDLK_d, 68 },
			{ SDLK_e, 69 },
			{ SDLK_f, 70 },
			{ SDLK_g, 71 },
			{ SDLK_h, 72 },
			{ SDLK_i, 73 },
			{ SDLK_j, 74 },
			{ SDLK_k, 75 },
			{ SDLK_l, 76 },
			{ SDLK_m, 77 },
			{ SDLK_n, 78 },
			{ SDLK_o, 79 },
			{ SDLK_p, 80 },
			{ SDLK_q, 81 },
			{ SDLK_r, 82 },
			{ SDLK_s, 83 },
			{ SDLK_t, 84 },
			{ SDLK_u, 85 },
			{ SDLK_v, 86 },
			{ SDLK_w, 87 },
			{ SDLK_x, 88 },
			{ SDLK_y, 89 },
			{ SDLK_z, 90 },
			{ SDLK_SELECT, 93 },
			{ SDLK_KP_0, 96 },
			{ SDLK_KP_1, 97 },
			{ SDLK_KP_2, 98 },
			{ SDLK_KP_3, 99 },
			{ SDLK_KP_4, 100 },
			{ SDLK_KP_5, 101 },
			{ SDLK_KP_6, 102 },
			{ SDLK_KP_7, 103 },
			{ SDLK_KP_8, 104 },
			{ SDLK_KP_9, 105 },
			{ SDLK_KP_MULTIPLY, 106 },
			{ SDLK_KP_PLUS, 107 },
			{ SDLK_KP_MINUS, 109 },
			{ SDLK_KP_DECIMAL, 110 }, 
			{ SDLK_KP_DIVIDE, 111 },
			{ SDLK_F1, 112 },
			{ SDLK_F2, 113 },
			{ SDLK_F3, 114 },
			{ SDLK_F4, 115 },
			{ SDLK_F5, 116 },
			{ SDLK_F6, 117 },
			{ SDLK_F7, 118 }, 
			{ SDLK_F8, 119 }, 
			{ SDLK_F9, 120 }, 
			{ SDLK_F10, 121 }, 
			{ SDLK_F11, 122 }, 
			{ SDLK_F12, 123 }, 
			{ SDLK_NUMLOCKCLEAR, 144 },
			{ SDLK_SCROLLLOCK, 145 },
			{ SDLK_SEMICOLON, 186 },
			{ SDLK_EQUALS, 187 },
			{ SDLK_COMMA, 188 },
			{ SDLK_MINUS, 189 },
			{ SDLK_PERIOD, 190 },
			{ SDLK_SLASH, 191 },
			{ SDLK_LEFTBRACKET, 219 },
			{ SDLK_BACKSLASH, 220 },
			{ SDLK_RIGHTBRACKET, 221 },
			{ SDLK_QUOTE, 222 },
			//{ LEFT MOUSE CLICK, 245 },
			//{ MIDDLE MOUSE CLICK, 246 },
			//{ RIGHT MOUSE CLICK, 247 },
			//{ MSCROLL_UP, 250 },
			//{ MSCROLL_DOWN, 251 },
		};

		if (e.type == SDL_KEYDOWN) {
			int keyCode = 0;
			if (e.key.keysym.sym == SDLK_LSHIFT || e.key.keysym.sym == SDLK_RSHIFT)
				keyCode = 16;
			else if (e.key.keysym.sym == SDLK_LCTRL || e.key.keysym.sym == SDLK_RCTRL)
				keyCode = 17;
			else if (e.key.keysym.sym == SDLK_LALT || e.key.keysym.sym == SDLK_RALT)
				keyCode = 18;
			else if (keyIDs.count(e.key.keysym.sym))
				keyCode = keyIDs[e.key.keysym.sym];

			if (keyCode > 0) {
				m_kbTexture[keyCode] = 0xFF;
				m_kbTexture[256 + keyCode] = 0xFF;
				m_kbTexture[512 + keyCode] = ~m_kbTexture[512 + keyCode];
			}
		} 
		else if (e.type == SDL_KEYUP) {
			int keyCode = -1;
			if (e.key.keysym.sym == SDLK_LSHIFT || e.key.keysym.sym == SDLK_RSHIFT)
				keyCode = 16;
			else if (e.key.keysym.sym == SDLK_LCTRL || e.key.keysym.sym == SDLK_RCTRL)
				keyCode = 17;
			else if (e.key.keysym.sym == SDLK_LALT || e.key.keysym.sym == SDLK_RALT)
				keyCode = 18;
			else if (keyIDs.count(e.key.keysym.sym))
				keyCode = keyIDs[e.key.keysym.sym];

			if (keyCode > 0)
				m_kbTexture[keyCode] = 0;
		} else if (e.type == SDL_MOUSEBUTTONDOWN) {
			int keyCode = -1;
			if (e.button.button == SDL_BUTTON_LEFT)
				keyCode = 245;
			else if (e.button.button == SDL_BUTTON_MIDDLE)
				keyCode = 246;
			else if (e.button.button == SDL_BUTTON_RIGHT)
				keyCode = 247;

			if (keyCode > 0) {
				m_kbTexture[keyCode] = 0xFF;
				m_kbTexture[256 + keyCode] = 0xFF;
				m_kbTexture[512 + keyCode] = ~m_kbTexture[512 + keyCode];
			}
		} else if (e.type == SDL_MOUSEBUTTONUP) {
			int keyCode = -1;
			if (e.button.button == SDL_BUTTON_LEFT)
				keyCode = 245;
			else if (e.button.button == SDL_BUTTON_MIDDLE)
				keyCode = 246;
			else if (e.button.button == SDL_BUTTON_RIGHT)
				keyCode = 247;

			if (keyCode > 0)
				m_kbTexture[keyCode] = 0;
		} else if (e.type == SDL_MOUSEWHEEL) {
			int keyCode = -1;
			if (e.wheel.y > 0)
				keyCode = 250;
			else if (e.wheel.y < 0)
				keyCode = 251;

			if (keyCode > 0) {
				m_kbTexture[256 + keyCode] = 0xFF;
				if (e.wheel.y > 0) {
					m_kbTexture[512 + keyCode]++;
					m_kbTexture[512 + keyCode + 1]++;
				} else if (e.wheel.y < 0) {
					m_kbTexture[512 + keyCode]--;
					m_kbTexture[512 + keyCode - 1]--;
				}
			}
		}
	}
	void ObjectManager::Update(float delta)
	{
		for (auto& it : m_itemData) {
			// update audio items
			if (it->SoundBuffer != nullptr) {
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
					m_audioTempTexData[i + ed::AudioAnalyzer::SampleCount] = sf * 0.5f + 0.5f;
				}

				glBindTexture(GL_TEXTURE_2D, it->Texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 512, 2, 0, GL_RED, GL_FLOAT, m_audioTempTexData);
				glBindTexture(GL_TEXTURE_2D, 0);
			}
			// update kb texture
			else if (it->IsKeyboardTexture) {
				glBindTexture(GL_TEXTURE_2D, it->Texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 256, 3, 0, GL_RED, GL_UNSIGNED_BYTE, m_kbTexture);
				glBindTexture(GL_TEXTURE_2D, 0);
				memset(&m_kbTexture[256], 0, sizeof(unsigned char) * 256);
			}
		}
	}
	void ObjectManager::Remove(const std::string& file)
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
			pobj->Owner->Object_Remove(file.c_str(), pobj->Type, pobj->Data, pobj->ID);
		}

		delete m_itemData[index];
		m_itemData.erase(m_itemData.begin() + index);
		m_items.erase(m_items.begin() + index);
	}

	void ObjectManager::Bind(const std::string& file, PipelineItem* pass)
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
	void ObjectManager::Unbind(const std::string& file, PipelineItem* pass)
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
	int ObjectManager::IsBound(const std::string& file, PipelineItem* pass)
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

	void ObjectManager::BindUniform(const std::string& file, PipelineItem* pass)
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
	void ObjectManager::UnbindUniform(const std::string& file, PipelineItem* pass)
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
	int ObjectManager::IsUniformBound(const std::string& file, PipelineItem* pass)
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
			if (item->Texture == texID || (item->Image != nullptr && item->Image->Texture == texID) || (item->Image3D != nullptr && item->Image3D->Texture == texID) || (item->Buffer != nullptr && item->Buffer->ID == texID)) {
				return m_items[i];
			}
		}

		return "";
	}
	glm::ivec2 ObjectManager::GetRenderTextureSize(const std::string& name)
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
	bool ObjectManager::IsTexture(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->IsTexture;
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
	void ObjectManager::UploadDataToImage(ImageObject* img, GLuint tex, glm::ivec2 texSize)
	{
		if (tex != 0 && texSize.x != 0 && texSize.y != 0) {
			int width = std::min<int>(img->Size.x, texSize.x);
			int height = std::min<int>(img->Size.y, texSize.y);

			bool needsResize = width != texSize.x;

			unsigned char* pixels = (unsigned char*)malloc(texSize.x * texSize.y * 4);
			unsigned char* resPixels = pixels;

			// read pixels
			glBindTexture(GL_TEXTURE_2D, tex);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

			if (needsResize) {
				resPixels = (unsigned char*)malloc(width * height * 4);
				for (int y = 0; y < height; y++)
					memcpy(&resPixels[(height - y - 1) * width * 4], &pixels[(texSize.y - y - 1) * texSize.x * 4], width * 4);
			}

			glBindTexture(GL_TEXTURE_2D, img->Texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, resPixels);
			glBindTexture(GL_TEXTURE_2D, 0);

			if (needsResize)
				free(resPixels);

			free(pixels);
		} else {
			unsigned char* pixels = (unsigned char*)calloc(img->Size.x * img->Size.y, 4);

			glBindTexture(GL_TEXTURE_2D, img->Texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img->Size.x, img->Size.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			glBindTexture(GL_TEXTURE_2D, 0);

			free(pixels);
		}
	}
	void ObjectManager::SaveToFile(const std::string& itemName, ObjectManagerItem* item, const std::string& filepath)
	{
		glm::vec2 imgSize = item->ImageSize;
		if (item->RT != nullptr)
			imgSize = GetRenderTextureSize(itemName);
		else if (item->Image != nullptr)
			imgSize = item->Image->Size;

		unsigned char* pixels = (unsigned char*)malloc(imgSize.x * imgSize.y * 4);

		GLuint outTex = item->Texture;
		if (item->Image != nullptr)
			outTex = item->Image->Texture; // TODO: why doesn't image use item->Texture ??

		glBindTexture(GL_TEXTURE_2D, outTex);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		glBindTexture(GL_TEXTURE_2D, 0);

		std::string ext = filepath.substr(filepath.find_last_of('.') + 1);

		if (ext == "jpg" || ext == "jpeg")
			stbi_write_jpg(filepath.c_str(), imgSize.x, imgSize.y, 4, pixels, 100);
		else if (ext == "bmp")
			stbi_write_bmp(filepath.c_str(), imgSize.x, imgSize.y, 4, pixels);
		else if (ext == "tga")
			stbi_write_tga(filepath.c_str(), imgSize.x, imgSize.y, 4, pixels);
		else
			stbi_write_png(filepath.c_str(), imgSize.x, imgSize.y, 4, pixels, imgSize.x * 4);

		free(pixels);
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
		return glm::ivec2(0, 0);
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
		return glm::ivec2(0, 0);
	}
	glm::ivec3 ObjectManager::GetImage3DSize(const std::string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == name)
				return m_itemData[i]->Image3D->Size;
		return glm::ivec3(0, 0, 0);
	}
	bool ObjectManager::HasKeyboardTexture()
	{
		for (const auto& obj : m_itemData)
			if (obj->IsKeyboardTexture)
				return true;
		return false;
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

	void ObjectManager::FlipTexture(const std::string& name)
	{
		ObjectManagerItem* item = GetObjectManagerItem(name);

		if (item != nullptr) {
			GLuint tex = item->Texture;
			for (auto& key : m_binds)
				for (int i = 0; i < key.second.size(); i++)
					if (key.second[i] == tex)
						key.second[i] = item->FlippedTexture;
			for (auto& key : m_uniformBinds)
				for (int i = 0; i < key.second.size(); i++)
					if (key.second[i] == tex)
						key.second[i] = item->FlippedTexture;

			item->Texture = item->FlippedTexture;
			item->FlippedTexture = tex;
			item->Texture_VFlipped = !item->Texture_VFlipped;
		}
	}
	void ObjectManager::UpdateTextureParameters(const std::string& name)
	{
		ObjectManagerItem* item = GetObjectManagerItem(name);

		if (item != nullptr) {
			glBindTexture(GL_TEXTURE_2D, item->Texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, item->Texture_MinFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, item->Texture_MagFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, item->Texture_WrapS);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, item->Texture_WrapT);

			glBindTexture(GL_TEXTURE_2D, item->FlippedTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, item->Texture_MinFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, item->Texture_MagFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, item->Texture_WrapS);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, item->Texture_WrapT);

			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	void ObjectManager::Mute(const std::string& name)
	{
		ObjectManagerItem* item = GetObjectManagerItem(name);
		if (item != nullptr) {
			item->SoundMuted = true;
			item->Sound->setVolume(0);
		}
	}
	void ObjectManager::Unmute(const std::string& name)
	{
		ObjectManagerItem* item = GetObjectManagerItem(name);
		if (item != nullptr) {
			item->SoundMuted = false;
			item->Sound->setVolume(100);
		}
	}

	void ObjectManager::ResizeRenderTexture(const std::string& name, glm::ivec2 size)
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