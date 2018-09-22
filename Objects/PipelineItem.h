#pragma once
#include <MoonLight/Base/Geometry.h>
#include <string>

namespace ed
{
	enum class PipelineItem
	{
		ShaderFile,
		Geometry,
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
	}
}