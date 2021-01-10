#pragma once

namespace ed {
	enum class ShaderStage {
		Vertex,
		Pixel,
		Geometry,
		Compute,

		Audio,
		Plugin,

		TessellationControl,
		TessellationEvaluation,

		Count
	};
}