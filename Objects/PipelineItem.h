#pragma once
#include <MoonLight/Base/Geometry.h>
#include <MoonLight/Base/Topology.h>
#include <MoonLight/Base/VertexShader.h>
#include <MoonLight/Base/VertexInputLayout.h>
#include <string>

#include "ShaderVariableContainer.h"

namespace ed
{
	enum class PipelineItem
	{
		ShaderPass,
		ShaderFile,
		Geometry
		/*	Model
			... */
	};

	namespace pipe
	{
		struct ShaderPass
		{
			char VSPath[512];
			ed::ShaderVariableContainer VSVariables;
		};

		struct ShaderItem
		{
			char FilePath[512];
			char Entry[32];
			ed::ShaderVariableContainer Variables;
			ml::VertexInputLayout InputLayout;
			enum ShaderType {
				PixelShader,
				VertexShader
			} Type;
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
	}
}