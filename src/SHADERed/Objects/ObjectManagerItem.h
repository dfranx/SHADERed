#pragma once
#include <string>
#include <glm/glm.hpp>
#include <SHADERed/Engine/AudioPlayer.h>

#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ed {
	enum class ObjectType {
		Unknown,
		RenderTexture,
		CubeMap,
		Audio,
		Buffer,
		Image,
		Texture,
		Image3D,
		KeyboardTexture,
		PluginObject,
		Texture3D
	};

	/* object information */
	struct RenderTextureObject {
		GLuint DepthStencilBuffer, DepthStencilBufferMS, BufferMS; // ColorBuffer is stored in ObjectManager
		glm::ivec2 FixedSize;
		glm::vec2 RatioSize;
		glm::vec4 ClearColor;
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
		bool PreviewPaused;
		GLuint ID;
	};
	struct ImageObject {
		glm::ivec2 Size;
		GLuint Format;
		char DataPath[SHADERED_MAX_PATH];
	};
	struct Image3DObject {
		glm::ivec3 Size;
		GLuint Format;
	};
	struct PluginObject {
		char Type[128];
		IPlugin1* Owner;

		GLuint ID;
		void* Data;
	};

	/* object - TODO: maybe inheritance? class ImageObject : public ObjectManagerItem -> though, too many changes */
	class ObjectManagerItem {
	public:
		ObjectManagerItem(const std::string& name, ObjectType type)
		{
			Name = name;
			Type = type;

			TextureSize = glm::ivec2(0, 0);
			Depth = 1;
			Texture = 0;
			FlippedTexture = 0;
			Texture_VFlipped = false;
			Texture_MinFilter = GL_LINEAR;
			Texture_MagFilter = GL_NEAREST;
			Texture_WrapS = GL_REPEAT;
			Texture_WrapT = GL_REPEAT;
			Texture_WrapR = GL_REPEAT;
			CubemapPaths.clear();
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
				free(Buffer->Data);
				delete Buffer;
			}
			if (Image != nullptr)
				delete Image;
			if (Image3D != nullptr)
				delete Image3D;

			if (RT != nullptr) {
				glDeleteTextures(1, &RT->DepthStencilBuffer);
				delete RT;
			}
			if (Sound != nullptr)
				delete Sound;
			if (Plugin != nullptr)
				delete Plugin;

			glDeleteTextures(1, &Texture);
			glDeleteTextures(1, &FlippedTexture);
			CubemapPaths.clear();
		}

		std::string Name;
		ObjectType Type;

		glm::ivec2 TextureSize;
		int Depth;
		GLuint Texture, FlippedTexture;
		std::vector<std::string> CubemapPaths;

		bool Texture_VFlipped;
		GLuint Texture_MinFilter, Texture_MagFilter, Texture_WrapS, Texture_WrapT, Texture_WrapR;

		eng::AudioPlayer* Sound;
		bool SoundMuted;

		RenderTextureObject* RT;
		BufferObject* Buffer;
		ImageObject* Image;
		Image3DObject* Image3D;

		PluginObject* Plugin;
	};
}