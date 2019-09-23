#pragma once
#include <string>

namespace ed
{
	enum class InputLayoutValue
	{
		Position,
		Normal,
		Texcoord,
		Tangent,
		Binormal,
		Color,
		MaxCount
	};

	class InputLayoutItem
	{
	public:
		InputLayoutValue Value;
		std::string Semantic;

		static size_t GetValueSize(InputLayoutValue val);
		static size_t GetValueOffset(InputLayoutValue val);
	};
}