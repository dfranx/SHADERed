#pragma once
#include "../Engine/Model.h"
#include "ShaderVariableContainer.h"
#include "../Options.h"

#include <glm/glm.hpp>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#include <GL/gl.h>
#include <string>

namespace ed
{
	struct PipelineItem
	{
		enum class ItemType
		{
			ShaderPass,
			Geometry,
			RenderState,
			Model,
			Count
		};

		PipelineItem() : Data(0) { memset(Name, 0, PIPELINE_ITEM_NAME_LENGTH * sizeof(char)); }
		PipelineItem(const char* name, ItemType type, void* data) {
			strcpy(Name, name);
			Type = type;
			Data = data;
		}

		char Name[PIPELINE_ITEM_NAME_LENGTH];
		ItemType Type;
		void* Data;
	};

	namespace pipe
	{
		struct ShaderPass
		{
			ShaderPass() { 
				memset(RenderTextures, 0, sizeof(GLuint) * MAX_RENDER_TEXTURES);
				DepthTexture = 0;
				FBO = 0;
				RTCount = 0;
				GSUsed = false;
				memset(VSPath, 0, sizeof(char) * MAX_PATH);
				memset(PSPath, 0, sizeof(char) * MAX_PATH);
				memset(GSPath, 0, sizeof(char) * MAX_PATH);
				memset(VSEntry, 0, sizeof(char) * 32);
				memset(PSEntry, 0, sizeof(char) * 32);
				memset(GSEntry, 0, sizeof(char) * 32);
			}

			GLbyte RTCount;
			GLuint RenderTextures[MAX_RENDER_TEXTURES];
			GLuint DepthTexture; // pointer to actual depth & stencil texture
			GLuint FBO; // actual framebuffer

			char VSPath[MAX_PATH];
			char VSEntry[32];

			char PSPath[MAX_PATH];
			char PSEntry[32];

			char GSPath[MAX_PATH];
			char GSEntry[32];
			bool GSUsed;

			ShaderVariableContainer Variables;

			std::vector<PipelineItem*> Items;
		};

		struct GeometryItem
		{
			GeometryItem()
			{
				Position = glm::vec3(0, 0, 0);
				Rotation = glm::vec3(0, 0, 0);
				Scale = glm::vec3(1, 1, 1);
				Size = glm::vec3(1, 1, 1);
				Topology = GL_TRIANGLES;
				Type = GeometryType::Cube;
				VAO = VBO = 0;
			}
			enum GeometryType {
				Cube,
				Rectangle,
				Circle,
				Triangle,
				Sphere,
				Plane
			} Type;

			GLuint VAO;
			GLuint VBO;
			unsigned int Topology;
			glm::vec3 Position, Rotation, Scale, Size;
		};

		struct RenderState
		{
			GLenum PolygonMode;
			bool CullFace;
			GLenum CullFaceType;
			GLenum FrontFace;

			bool Blend;
			bool AlphaToCoverage;
			GLenum BlendSourceFactorRGB;
			GLenum BlendDestinationFactorRGB;
			GLenum BlendFunctionColor;
			GLenum BlendSourceFactorAlpha;
			GLenum BlendDestinationFactorAlpha;
			GLenum BlendFunctionAlpha;
			glm::vec4 BlendFactor;

			bool DepthTest;
			bool DepthClamp;
			bool DepthMask;
			GLenum DepthFunction;
			GLfloat DepthBias;

			bool StencilTest;
			GLuint StencilMask;
			GLenum StencilFrontFaceFunction, StencilBackFaceFunction;
			GLuint StencilReference;
			GLenum StencilFrontFaceOpStencilFail, StencilFrontFaceOpDepthFail, StencilFrontFaceOpPass;
			GLenum StencilBackFaceOpStencilFail, StencilBackFaceOpDepthFail, StencilBackFaceOpPass;

			RenderState() {
				PolygonMode = GL_FILL;
				CullFace = true;
				CullFaceType = GL_BACK;
				FrontFace = GL_CCW;

				Blend = false;
				AlphaToCoverage = false;
				BlendSourceFactorRGB = GL_SRC_ALPHA;
				BlendDestinationFactorRGB = GL_ONE_MINUS_SRC_ALPHA;
				BlendFunctionColor = GL_FUNC_ADD; 
				BlendSourceFactorAlpha = GL_SRC_ALPHA;
				BlendDestinationFactorAlpha = GL_ONE_MINUS_SRC_ALPHA;
				BlendFunctionAlpha = GL_FUNC_ADD;
				BlendFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

				DepthTest = true;
				DepthClamp = true;
				DepthMask = true;
				DepthFunction = GL_LESS;
				DepthBias = 0.0f;

				StencilTest = false;
				StencilMask = 0x00;
				StencilFrontFaceFunction = StencilBackFaceFunction = GL_EQUAL;
				StencilReference = 0xFF;
				StencilFrontFaceOpPass = StencilFrontFaceOpDepthFail = StencilFrontFaceOpStencilFail = GL_KEEP;
				StencilBackFaceOpPass = StencilBackFaceOpDepthFail = StencilBackFaceOpStencilFail = GL_KEEP;
			}
		};

		struct Model
		{
			bool OnlyGroup; // render only a group
			char GroupName[MODEL_GROUP_NAME_LENGTH];
			char Filename[MAX_PATH];
			
			eng::Model* Data;

			glm::vec3 Position, Rotation, Scale;
		};
	}
}