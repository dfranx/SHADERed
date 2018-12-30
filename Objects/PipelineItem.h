#pragma once
#include <MoonLight/Base/Geometry.h>
#include <MoonLight/Base/Topology.h>
#include <MoonLight/Base/BlendState.h>
#include <MoonLight/Base/VertexShader.h>
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
			/*	Model
				... */
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
			char VSPath[512];
			char VSEntry[32];
			ml::VertexInputLayout VSInputLayout;
			ShaderVariableContainer VSVariables;

			char PSPath[512];
			char PSEntry[32];
			ShaderVariableContainer PSVariables;

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
				Cube
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
	}
}