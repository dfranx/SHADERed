#pragma once
#include <MoonLight/Model/OBJModel.h>
#include <MoonLight/Base/Geometry.h>
#include <MoonLight/Base/Topology.h>
#include <MoonLight/Base/BlendState.h>
#include <MoonLight/Base/VertexShader.h>
#include <MoonLight/Base/RasterizerState.h>
#include <MoonLight/Base/VertexInputLayout.h>
#include <MoonLight/Base/DepthStencilState.h>
#include <string>

#include "ShaderVariableContainer.h"

namespace ed
{
	struct PipelineItem
	{
		enum class ItemType
		{
			ShaderPass,
			Geometry,
			BlendState,
			DepthStencilState,
			RasterizerState,
			OBJModel,
			Count
		};

		char Name[PIPELINE_ITEM_NAME_LENGTH];
		ItemType Type;
		void* Data;
	};

	namespace pipe
	{
		struct ShaderPass
		{
			char RenderTexture[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT][64];

			char VSPath[MAX_PATH];
			char VSEntry[32];
			ml::VertexInputLayout VSInputLayout;
			ShaderVariableContainer VSVariables;

			char PSPath[MAX_PATH];
			char PSEntry[32];
			ShaderVariableContainer PSVariables;

			char GSPath[MAX_PATH];
			char GSEntry[32];
			ShaderVariableContainer GSVariables;
			bool GSUsed;

			std::vector<PipelineItem*> Items;
		};

		struct GeometryItem
		{
			GeometryItem()
			{
				Position = DirectX::XMFLOAT3(0, 0, 0);
				Rotation = DirectX::XMFLOAT3(0, 0, 0);
				Scale = DirectX::XMFLOAT3(1, 1, 1);
				Size = DirectX::XMFLOAT3(1, 1, 1);
			}
			enum GeometryType {
				Cube,
				Rectangle,
				Circle,
				Triangle,
				Sphere,
				Plane
			} Type;

			ml::Geometry Geometry;
			ml::Topology::Type Topology;
			DirectX::XMFLOAT3 Position, Rotation, Scale, Size;
		};

		struct BlendState
		{
			ml::BlendState State;
		};

		struct DepthStencilState
		{
			ml::DepthStencilState State;
			ml::UInt32 StencilReference;
		};

		struct RasterizerState
		{
			ml::RasterizerState State;
		};

		struct OBJModel
		{
			bool OnlyGroup; // render only a group
			char GroupName[MODEL_GROUP_NAME_LENGTH];
			char Filename[MAX_PATH];
			ml::UInt32 VertCount;
			ml::VertexBuffer<ml::OBJModel::Vertex> Vertices;

			ml::OBJModel Mesh;
			DirectX::XMFLOAT3 Position, Rotation, Scale;
		};
	}
}