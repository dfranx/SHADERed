#pragma once

// NAMES //
extern const char* TOPOLOGY_ITEM_NAMES[10];
extern const char* SYSTEM_VARIABLE_NAMES[20];
extern const char* VARIABLE_TYPE_NAMES[15];
extern const char* FUNCTION_NAMES[23];
extern const char* GEOMETRY_NAMES[7];
extern const char* PIPELINE_ITEM_NAMES[7];
extern const char* BLEND_NAMES[20];
extern const char* BLEND_OPERATOR_NAMES[6];
extern const char* COMPARISON_FUNCTION_NAMES[9];
extern const char* STENCIL_OPERATION_NAMES[9];
extern const char* CULL_MODE_NAMES[4];
extern const char* FORMAT_NAMES[66];
extern const char* ATTRIBUTE_VALUE_NAMES[6];
extern const char* EDITOR_SHORTCUT_NAMES[48];

// VALUES //
extern const unsigned int FORMAT_VALUES[66];
extern const unsigned int BLEND_VALUES[20];
extern const unsigned int BLEND_OPERATOR_VALUES[6];
extern const unsigned int COMPARISON_FUNCTION_VALUES[9];
extern const unsigned int STENCIL_OPERATION_VALUES[9];
extern const unsigned int CULL_MODE_VALUES[4];
extern const unsigned int TOPOLOGY_ITEM_VALUES[10];

namespace ed
{
	namespace gl
	{
		class String
		{
		public:
			static const char* Format(unsigned int val);
			static const char* BlendFactor(unsigned int val);
			static const char* BlendFunction(unsigned int val);
			static const char* ComparisonFunction(unsigned int val);
			static const char* StencilOperation(unsigned int val);
		};
	}
}

