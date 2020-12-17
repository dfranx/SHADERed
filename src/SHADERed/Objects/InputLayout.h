#pragma once
#include <string>

namespace ed {
	enum class InputLayoutValue {
		Position,
		Normal,
		Texcoord,
		Tangent,
		Binormal,
		Color,
		BufferFloat,
		BufferFloat2,
		BufferFloat3,
		BufferFloat4,
		BufferInt,
		BufferInt2,
		BufferInt3,
		BufferInt4,
		MaxCount
	};

	class InputLayoutItem {
	public:
		InputLayoutValue Value;
		std::string Semantic;

		static size_t GetValueSize(InputLayoutValue val);
		static size_t GetValueOffset(InputLayoutValue val);
	};
}