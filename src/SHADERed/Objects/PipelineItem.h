#pragma once
#include <SHADERed/Engine/Model.h>
#include <SHADERed/Objects/AudioShaderStream.h>
#include <SHADERed/Objects/InputLayout.h>
#include <SHADERed/Objects/ShaderMacro.h>
#include <SHADERed/Objects/ShaderVariableContainer.h>
#include <SHADERed/Options.h>

#include <glm/glm.hpp>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <string>

namespace ed {
	struct PipelineItem {
		enum class ItemType {
			ShaderPass,
			Geometry,
			RenderState,
			Model,
			VertexBuffer,
			ComputePass,
			AudioPass,
			PluginItem,
			Count
		};

		PipelineItem()
				: Data(0)
		{
			memset(Name, 0, PIPELINE_ITEM_NAME_LENGTH * sizeof(char));
		}
		PipelineItem(const char* name, ItemType type, void* data)
		{
			strcpy(Name, name);
			Type = type;
			Data = data;
		}

		char Name[PIPELINE_ITEM_NAME_LENGTH];
		ItemType Type;
		void* Data;
	};

	namespace pipe {
		struct PluginItemData {
			char Type[64];
			IPlugin1* Owner;
			void* PluginData;

			std::vector<PipelineItem*> Items;
		};

		struct ComputePass {
			ComputePass()
			{
				Macros.clear();
				memset(Path, 0, sizeof(char) * SHADERED_MAX_PATH);
				memset(Entry, 0, sizeof(char) * 32);

				WorkX = WorkY = WorkZ = 1;
				Active = true;
			}

			char Path[SHADERED_MAX_PATH];
			char Entry[32];
			std::vector<unsigned int> SPV; // SPIR-V

			bool Active;

			GLuint WorkX, WorkY, WorkZ;
			ShaderVariableContainer Variables;
			std::vector<ShaderMacro> Macros;
		};

		struct AudioPass {
			AudioPass()
			{
				Macros.clear();
				memset(Path, 0, sizeof(char) * SHADERED_MAX_PATH);
			}

			ed::AudioShaderStream Stream;
			char Path[SHADERED_MAX_PATH];
			ShaderVariableContainer Variables;
			std::vector<ShaderMacro> Macros;
		};

		struct ShaderPass {
			ShaderPass()
			{
				memset(RenderTextures, 0, sizeof(GLuint) * MAX_RENDER_TEXTURES);
				DepthTexture = 0;
				FBO = 0;
				RTCount = 0;
				GSUsed = false;
				TSUsed = false;
				TSPatchVertices = 1;
				Active = true;
				Macros.clear();
				memset(VSPath, 0, sizeof(char) * SHADERED_MAX_PATH);
				memset(PSPath, 0, sizeof(char) * SHADERED_MAX_PATH);
				memset(GSPath, 0, sizeof(char) * SHADERED_MAX_PATH);
				memset(TCSPath, 0, sizeof(char) * SHADERED_MAX_PATH);
				memset(TESPath, 0, sizeof(char) * SHADERED_MAX_PATH);
				memset(VSEntry, 0, sizeof(char) * 32);
				memset(PSEntry, 0, sizeof(char) * 32);
				memset(GSEntry, 0, sizeof(char) * 32);
				memset(TCSEntry, 0, sizeof(char) * 32);
				memset(TESEntry, 0, sizeof(char) * 32);
			}

			GLbyte RTCount;
			GLuint RenderTextures[MAX_RENDER_TEXTURES];
			GLuint DepthTexture; // pointer to actual depth & stencil texture
			GLuint FBO;			 // actual framebuffer

			bool Active;

			char VSPath[SHADERED_MAX_PATH];
			char VSEntry[32];
			std::vector<unsigned int> VSSPV; // VS SPIR-V

			char PSPath[SHADERED_MAX_PATH];
			char PSEntry[32];
			std::vector<unsigned int> PSSPV; // PS SPIR-V

			char GSPath[SHADERED_MAX_PATH];
			char GSEntry[32];
			std::vector<unsigned int> GSSPV; // GS SPIR-V
			bool GSUsed;

			bool TSUsed;
			int TSPatchVertices;
			// Tessellation control shader
			char TCSPath[SHADERED_MAX_PATH];
			char TCSEntry[32];
			std::vector<unsigned int> TCSSPV; // Tessellation control shader SPIR-V
			// Tessellation evaluation shader
			char TESPath[SHADERED_MAX_PATH];
			char TESEntry[32];
			std::vector<unsigned int> TESSPV; // Tessellation evaulation shader SPIR-V

			ShaderVariableContainer Variables;
			std::vector<ShaderMacro> Macros;

			std::vector<InputLayoutItem> InputLayout;

			std::vector<PipelineItem*> Items;
		};

		struct GeometryItem {
			GeometryItem()
			{
				Position = glm::vec3(0, 0, 0);
				Rotation = glm::vec3(0, 0, 0);
				Scale = glm::vec3(1, 1, 1);
				Size = glm::vec3(1, 1, 1);
				Topology = GL_TRIANGLES;
				Type = GeometryType::Cube;
				VAO = VBO = 0;
				Instanced = false;
				InstanceCount = 0;
				InstanceBuffer = nullptr;
			}
			enum GeometryType {
				Cube,
				Rectangle, // ScreenQuad
				Circle,
				Triangle,
				Sphere,
				Plane,
				ScreenQuadNDC,
				Count
			} Type;

			GLuint VAO;
			GLuint VBO;
			unsigned int Topology;
			glm::vec3 Position, Rotation, Scale, Size;

			bool Instanced;
			int InstanceCount;
			void* InstanceBuffer;
		};

		struct VertexBuffer {
			VertexBuffer()
			{
				Position = glm::vec3(0, 0, 0);
				Rotation = glm::vec3(0, 0, 0);
				Scale = glm::vec3(1, 1, 1);
				Topology = GL_TRIANGLES;
				Buffer = 0;
				VAO = 0;
				Instanced = false;
				InstanceCount = 0;
				InstanceBuffer = nullptr;
			}

			void* Buffer;
			GLuint VAO;

			unsigned int Topology;
			glm::vec3 Position, Rotation, Scale;

			bool Instanced;
			int InstanceCount;
			void* InstanceBuffer;
		};

		struct RenderState {
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

			RenderState()
			{
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

		struct Model {
			bool OnlyGroup; // render only a group
			char GroupName[MODEL_GROUP_NAME_LENGTH];
			char Filename[SHADERED_MAX_PATH];

			eng::Model* Data;

			glm::vec3 Position, Rotation, Scale;

			bool Instanced;
			int InstanceCount;
			void* InstanceBuffer;
		};
	}
}