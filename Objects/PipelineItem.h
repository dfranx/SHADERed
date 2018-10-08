#pragma once
#include <MoonLight/Base/Geometry.h>
#include <MoonLight/Base/Topology.h>
#include <MoonLight/Base/VertexShader.h>
#include <MoonLight/Base/VertexInputLayout.h>
#include <string>

namespace ed
{
	enum class PipelineItem
	{
		ShaderFile,
		Geometry
		/*	Model
			... */
	};

	namespace pipe
	{
		struct ShaderItem
		{
			char FilePath[512];
			char Entry[32];
			ml::VertexInputLayout InputLayout;
			enum ShaderType {
				PixelShader,
				VertexShader
			} Type;
		};

		struct GeometryItem
		{
			ml::Geometry Geometry;
			ml::Topology::Type Topology;
			DirectX::XMFLOAT3 Position, Rotation, Scale;
		};
	}
}