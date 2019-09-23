#include "InputLayout.h"

namespace ed
{
	size_t InputLayoutItem::GetValueSize(InputLayoutValue val)
	{
		switch (val) {
			case InputLayoutValue::Position: return 3;
			case InputLayoutValue::Normal: return 3;
			case InputLayoutValue::Texcoord: return 2;
			case InputLayoutValue::Tangent: return 3;
			case InputLayoutValue::Binormal: return 3;
			case InputLayoutValue::Color: return 4;
		}
	}
	size_t InputLayoutItem::GetValueOffset(InputLayoutValue val)
	{
		switch (val) {
			case InputLayoutValue::Position: return 0;
			case InputLayoutValue::Normal: return 3;
			case InputLayoutValue::Texcoord: return 6;
			case InputLayoutValue::Tangent: return 8;
			case InputLayoutValue::Binormal: return 11;
			case InputLayoutValue::Color: return 14;
		}
	}
}