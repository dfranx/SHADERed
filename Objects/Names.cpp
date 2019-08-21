#include "Names.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#include <GL/gl.h>

const char* TOPOLOGY_ITEM_NAMES[] =
{
	"Undefined",
	"PointList",
	"LineList",
	"LineStrip",
	"TriangleList",
	"TriangleStrip",
	"LineListAdjecent",
	"LineStripAdjecent",
	"TriangleListAdjecent",
	"TriangleStripAdjecent"
};

const char* SHADER_TYPE_NAMES[] =
{
	"Pixel",
	"Vertex"
};

const char* SYSTEM_VARIABLE_NAMES[] = {
	"--NONE--",
	"Time",
	"TimeDelta",
	"ViewportSize",
	"MousePosition",
	"View",
	"Projection",
	"ViewProjection",
	"Orthographic",
	"ViewOrthographic",
	"GeometryTransform",
	"IsPicked",
	"CameraPosition",
	"KeysWASD"
};
const char* VARIABLE_TYPE_NAMES[] = {
	"bool",
	"bool2",
	"bool3",
	"bool4",
	"int",
	"int2",
	"int3",
	"int4",
	"float",
	"float2",
	"float3",
	"float4",
	"float2x2",
	"float3x3",
	"float4x4"
};

const char* FUNCTION_NAMES[] = {
	"None",
	"MatrixIdentity",
	"MatrixLookAtLH",
	"MatrixLookToLH",
	"MatrixOrthographicLH",
	"MatrixPerspectiveFovLH",
	"MatrixPerspectiveLH",
	"MatrixReflect",
	"MatrixRotationAxis",
	"MatrixRotationNormal",
	"MatrixRotationRollPitchYaw",
	"MatrixRotationX",
	"MatrixRotationY",
	"MatrixRotationZ",
	"MatrixScaling",
	"MatrixShadow",
	"MatrixTranslation",
	"ScalarCos",
	"ScalarSin",
	"VectorNormalize"
};

const char* GEOMETRY_NAMES[] = {
	"Cube",
	"ScreenQuad",
	"Circle",
	"Triangle",
	"Sphere",
	"Plane"
};

const char* PIPELINE_ITEM_NAMES[] =
{
	"Shader Pass",
	"Geometry"
};

const char* BLEND_NAMES[] = {
	"",
	"Zero",
	"One",
	"Source Color",
	"Inverse Source Color",
	"Source Alpha",
	"Inverse Source Alpha",
	"Destination Alpha",
	"Inverse Destination Alpha",
	"Destination Color",
	"Inverse Destination Color",
	"Saturated Source Alpha",  // 11
	"", "",
	"Blend Factor",
	"Inverse Blend Factor",
	"Source 1 Color",
	"Inverse Source 1 Color",
	"Source 1 Alpha",
	"Inverse Source 1 Alpha"
};
const char* BLEND_OPERATOR_NAMES[] = {
	"",
	"Add",
	"Subtract",
	"Reverse Subtract",
	"Min",
	"Max"
};
const char* COMPARISON_FUNCTION_NAMES[] = {
	"",
	"Never",
	"Less",
	"Equal",
	"LessEqual",
	"Greater",
	"NotEqual",
	"GreaterEqual",
	"Always"
};
const char* STENCIL_OPERATION_NAMES[] = {
	"",
	"Keep",
	"Zero",
	"Replace",
	"Increase Saturated",
	"Decrease Saturated",
	"Invert",
	"Increase",
	"Decrease"
};
const char* CULL_MODE_NAMES[] = {
	"",
	"None",
	"Front",
	"Back"
};



const unsigned int BLEND_VALUES[] = {
	-1,
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_SRC_ALPHA_SATURATE,  // 11
	-1, -1,
	GL_CONSTANT_COLOR,
	GL_ONE_MINUS_CONSTANT_COLOR,
	-1, -1,
	-1, -1,
};
const unsigned int BLEND_OPERATOR_VALUES[] = {
	-1,
	GL_FUNC_ADD,
	GL_FUNC_SUBTRACT,
	GL_FUNC_REVERSE_SUBTRACT,
	GL_MIN,
	GL_MAX
};
const unsigned int COMPARISON_FUNCTION_VALUES[] = {
	-1,
	GL_NEVER,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS
};
const unsigned int STENCIL_OPERATION_VALUES[] = {
	GL_ZERO,
	GL_KEEP,
	GL_ZERO,
	GL_REPLACE,
	GL_INCR_WRAP,
	GL_DECR_WRAP,
	GL_INVERT,
	GL_INCR,
	GL_DECR
};
const unsigned int CULL_MODE_VALUES[] = {
	-1,
	GL_ZERO,
	GL_FRONT,
	GL_BACK
};
const unsigned int TOPOLOGY_ITEM_VALUES[] =
{
	-1, // "Undefined"
	GL_POINTS,
	GL_LINES,
	GL_LINE_STRIP,
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_LINES_ADJACENCY,
	GL_LINE_STRIP_ADJACENCY,
	GL_TRIANGLES_ADJACENCY,
	GL_TRIANGLE_STRIP_ADJACENCY
};

namespace ed
{
	namespace gl
	{
		const char* String::BlendFactor(unsigned int val)
		{
			int len = (sizeof(BLEND_VALUES) / sizeof(*BLEND_VALUES));

			for (int i = 0; i < len; i++)
				if (val == BLEND_VALUES[i])
					return BLEND_NAMES[i];
			return BLEND_NAMES[0];
		}
		const char* String::BlendFunction(unsigned int val)
		{
			int len = (sizeof(BLEND_OPERATOR_VALUES) / sizeof(*BLEND_OPERATOR_VALUES));

			for (int i = 0; i < len; i++)
				if (val == BLEND_OPERATOR_VALUES[i])
					return BLEND_OPERATOR_NAMES[i];
			return BLEND_OPERATOR_NAMES[0];
		}
		const char* String::ComparisonFunction(unsigned int val)
		{
			int len = (sizeof(COMPARISON_FUNCTION_VALUES) / sizeof(*COMPARISON_FUNCTION_VALUES));

			for (int i = 0; i < len; i++)
				if (val == COMPARISON_FUNCTION_VALUES[i])
					return COMPARISON_FUNCTION_NAMES[i];
			return COMPARISON_FUNCTION_NAMES[0];
		}
		const char* String::StencilOperation(unsigned int val)
		{
			int len = (sizeof(STENCIL_OPERATION_VALUES) / sizeof(*STENCIL_OPERATION_VALUES));

			for (int i = 0; i < len; i++)
				if (val == STENCIL_OPERATION_VALUES[i])
					return STENCIL_OPERATION_NAMES[i];
			return STENCIL_OPERATION_NAMES[0];
		}
	}
}