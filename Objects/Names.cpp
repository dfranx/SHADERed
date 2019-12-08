#include "Names.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#if defined(__APPLE__)
	#include <OpenGL/gl.h>
#else
	#include <GL/gl.h>
#endif

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
	"FrameIndex",
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
	"CameraPosition3",
	"CameraDirection3",
	"KeysWASD",
	"Mouse",
	"MouseButton",
	"PluginVariable"
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
	"Pointer",
	"CameraSnapshot",
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
	"VectorNormalize",
	"PluginFunction"
};

const char* GEOMETRY_NAMES[] = {
	"Cube",
	"ScreenQuad",
	"Circle",
	"Triangle",
	"Sphere",
	"Plane",
	"ScreenQuadNDC"
};

const char* PIPELINE_ITEM_NAMES[] =
{
	"Shader Pass",
	"Geometry",
	"Render State",
	"Model",
	"Compute Pass",
	"Audio Pass",
	"Plugin Item"
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
const char* FORMAT_NAMES[] = {
	"UNKNOWN",
	"RGBA",
	"RGB",
	"RG",
	"R",
	"R8",
	"R8_SNORM",
	"R16",
	"R16_SNORM",
	"R8G8",
	"R8G8_SNORM",
	"R16G16",
	"R16G16_SNORM",
	"R3G3B2",
	"R4G4B4",
	"R5G5B5",
	"R8G8B8",
	"R8G8B8_SNORM",
	"R10G10B10",
	"R12G12B12",
	"R16G16B16_SNORM",
	"R2G2B2A2",
	"R4G4B4A4",
	"R5G5B5A1",
	"R8G8B8A8",
	"R8G8B8A8_SNORM",
	"R10G10B10A2",
	"R10G10B10A2_UINT",
	"R12G12B12A12",
	"R16G16B16A16",
	"SR8G8B8",
	"SR8G8B8A8",
	"R16_FLOAT",
	"R16G16_FLOAT",
	"R16G16B16_FLOAT",
	"R16G16B16A16_FLOAT",
	"R32_FLOAT",
	"R32G32_FLOAT",
	"R32G32B32_FLOAT",
	"R32G32B32A32_FLOAT",
	"R11G11B10_FLOAT",
	"R9G9B9_E5",
	"R8_INT",
	"R8_UINT",
	"R16_INT",
	"R16_UINT",
	"R32_INT",
	"R32_UINT",
	"R8G8_INT",
	"R8G8_UINT",
	"R16G16_INT",
	"R16G16_UINT",
	"R32G32_INT",
	"R32G32_UINT",
	"R8G8B8_INT",
	"R8G8B8_UINT",
	"R16G16B16_INT",
	"R16G16B16_UINT",
	"R32G32B32_INT",
	"R32G32B32_UINT",
	"R8G8B8A8_INT",
	"R8G8B8A8_UINT",
	"R16G16B16A16_INT",
	"R16G16B16A16_UINT",
	"R32G32B32A32_INT",
	"R32G32B32A32_UINT"
};
const char* ATTRIBUTE_VALUE_NAMES[] = {
	"Position",
	"Normal",
	"Texcoord",
	"Tangent",
	"Bitangent",
	"Color"
};
const char* EDITOR_SHORTCUT_NAMES[] =
{
	"Undo",
	"Redo",
	"MoveUp",
	"SelectUp",
	"MoveDown",
	"SelectDown",
	"MoveLeft",
	"SelectLeft",
	"MoveWordLeft",
	"SelectWordLeft",
	"MoveRight",
	"SelectRight",
	"MoveWordRight",
	"SelectWordRight",
	"MoveUpBlock",
	"SelectUpBlock",
	"MoveDownBlock",
	"SelectDownBlock",
	"MoveTop",
	"SelectTop",
	"MoveBottom",
	"SelectBottom",
	"MoveStartLine",
	"SelectStartLine",
	"MoveEndLine",
	"SelectEndLine",
	"ForwardDelete",
	"ForwardDeleteWord",
	"DeleteRight",
	"BackwardDelete",
	"BackwardDeleteWord",
	"DeleteLeft",
	"OverwriteCursor",
	"Copy",
	"Paste",
	"Cut",
	"SelectAll",
	"AutocompleteOpen",
	"AutocompleteSelect",
	"AutocompleteSelectActive",
	"AutocompleteUp",
	"AutocompleteDown",
	"NewLine",
	"Indent",
	"Unindent",
	"Find",
	"Replace",
	"FindNext"
};



const unsigned int FORMAT_VALUES[] = {
	-1,
	GL_RGBA,
	GL_RGB,
	GL_RG,
	GL_RED,
	GL_R8,
	GL_R8_SNORM,
	GL_R16,
	GL_R16_SNORM,
	GL_RG8,
	GL_RG8_SNORM,
	GL_RG16,
	GL_RG16_SNORM,
	GL_R3_G3_B2,
	GL_RGB4,
	GL_RGB5,
	GL_RGB8,
	GL_RGB8_SNORM,
	GL_RGB10,
	GL_RGB12,
	GL_RGB16_SNORM,
	GL_RGBA2,
	GL_RGBA4,
	GL_RGB5_A1,
	GL_RGBA8,
	GL_RGBA8_SNORM,
	GL_RGB10_A2,
	GL_RGB10_A2UI,
	GL_RGBA12,
	GL_RGBA16,
	GL_SRGB8,
	GL_SRGB8_ALPHA8,
	GL_R16F,
	GL_RG16F,
	GL_RGB16F,
	GL_RGBA16F,
	GL_R32F,
	GL_RG32F,
	GL_RGB32F,
	GL_RGBA32F,
	GL_R11F_G11F_B10F,
	GL_RGB9_E5,
	GL_R8I,
	GL_R8UI,
	GL_R16I,
	GL_R16UI,
	GL_R32I,
	GL_R32UI,
	GL_RG8I,
	GL_RG8UI,
	GL_RG16I,
	GL_RG16UI,
	GL_RG32I,
	GL_RG32UI,
	GL_RGB8I,
	GL_RGB8UI,
	GL_RGB16I,
	GL_RGB16UI,
	GL_RGB32I,
	GL_RGB32UI,
	GL_RGBA8I,
	GL_RGBA8UI,
	GL_RGBA16I,
	GL_RGBA16UI,
	GL_RGBA32I,
	GL_RGBA32UI
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
		const char* String::Format(unsigned int val)
		{
			int len = (sizeof(FORMAT_VALUES) / sizeof(*FORMAT_VALUES));

			for (int i = 0; i < len; i++)
				if (val == FORMAT_VALUES[i])
					return FORMAT_NAMES[i];
			return FORMAT_NAMES[0];
		}
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