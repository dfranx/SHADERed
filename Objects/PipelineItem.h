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
		Geometry,
		PrimitiveTopology,
		InputLayout
		/*	Model
			... */
	};

	namespace pipe
	{
		struct ShaderItem
		{
			char FilePath[512];
			enum ShaderType {
				PixelShader,
				VertexShader,
				GeometryShader
			} Type;
		};

		struct GeometryItem
		{
			ml::Geometry Geometry;
			DirectX::XMFLOAT3 Position, Rotation, Scale;
		};

		struct PrimitiveTopology
		{
			ml::Topology::Type Type;
		};

		struct InputLayout
		{
			ml::VertexInputLayout Layout;
			ShaderItem* Shader;
		};
	}
}