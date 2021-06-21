#include <SHADERed/Objects/Names.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

const char* TOPOLOGY_ITEM_NAMES[] = {
	"Undefined",
	"PointList",
	"LineList",
	"LineStrip",
	"TriangleList",
	"TriangleStrip",
	"LineListAdjecent",
	"LineStripAdjecent",
	"TriangleListAdjecent",
	"TriangleStripAdjecent",
	"Patches"
};
// https://www.khronos.org/opengl/wiki/Geometry_Shader
// https://www.khronos.org/opengl/wiki/Primitive#Patches // TODO patches vertices count depends on tess shaders
const unsigned char TOPOLOGY_SINGLE_VERTEX_COUNT[] = {
	0, 1, 2, 2, 3, 3, 4, 4, 6, 6, 3
};
const unsigned char TOPOLOGY_IS_STRIP[] = {
	0, 0, 0, 1, 0, 2, 0, 1, 0, 2, 0
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
	"IsSavingToFile",
	"CameraPosition",
	"CameraPosition3",
	"CameraDirection3",
	"KeysWASD",
	"Mouse",
	"MouseButton",
	"PickPosition",
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
const char* VARIABLE_TYPE_NAMES_GLSL[] = {
	"bool",
	"bvec2",
	"bvec3",
	"bvec4",
	"int",
	"ivec2",
	"ivec3",
	"ivec4",
	"float",
	"vec2",
	"vec3",
	"vec4",
	"mat2",
	"mat3",
	"mat4"
};

const char* FUNCTION_NAMES[] = {
	"None",
	"Pointer",
	"CameraSnapshot",
	"ObjectProperty",
	"MatrixIdentity",
	"MatrixLookAtLH",
	"MatrixLookToLH",
	"MatrixOrthographicLH",
	"MatrixPerspectiveFovLH",
	"MatrixPerspectiveLH",
	"MatrixRotationAxis",
	"MatrixRotationNormal",
	"MatrixRotationRollPitchYaw",
	"MatrixRotationX",
	"MatrixRotationY",
	"MatrixRotationZ",
	"MatrixScaling",
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

const char* PIPELINE_ITEM_NAMES[] = {
	"Shader Pass",
	"Geometry",
	"Render State",
	"Model",
	"Vertex Buffer",
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
	"Saturated Source Alpha", // 11
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
const char* TEXTURE_MIN_FILTER_NAMES[] = {
	"Nearest",
	"Linear",
	"Nearest_MipmapNearest",
	"Linear_MipmapNearest",
	"Nearest_MipmapLinear",
	"Linear_MipmapLinear"
};
const char* TEXTURE_MAG_FILTER_NAMES[] = {
	"Nearest",
	"Linear"
};
const char* TEXTURE_WRAP_NAMES[] = {
	"ClampToEdge",
	"Repeat",
	"MirroredRepeat"
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
	"Color",
	"BufferFloat",
	"BufferFloat2",
	"BufferFloat3",
	"BufferFloat4",
	"BufferInt",
	"BufferInt2",
	"BufferInt3",
	"BufferInt4"
};
const char* EDITOR_SHORTCUT_NAMES[] = {
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
	"FindNext",
	"DebugStep",
	"DebugStepInto",
	"DebugStepOut",
	"DebugContinue",
	"DebugJumpHere",
	"DebugBreakpoint",
	"DebugStop",
	"DuplicateLine",
	"CommentLines",
	"UncommentLines"
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
	GL_SRC_ALPHA_SATURATE, // 11
	-1,
	-1,
	GL_CONSTANT_COLOR,
	GL_ONE_MINUS_CONSTANT_COLOR,
	-1,
	-1,
	-1,
	-1,
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
const unsigned int TEXTURE_MIN_FILTER_VALUES[] = {
	GL_NEAREST,
	GL_LINEAR,
	GL_NEAREST_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_NEAREST,
	GL_NEAREST_MIPMAP_LINEAR,
	GL_LINEAR_MIPMAP_LINEAR
};
const unsigned int TEXTURE_MAG_FILTER_VALUES[] = {
	GL_NEAREST,
	GL_LINEAR,
};
const unsigned int TEXTURE_WRAP_VALUES[] = {
	GL_CLAMP_TO_EDGE,
	GL_REPEAT,
	GL_MIRRORED_REPEAT
};
const unsigned int TOPOLOGY_ITEM_VALUES[] = {
	-1, // "Undefined"
	GL_POINTS,
	GL_LINES,
	GL_LINE_STRIP,
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_LINES_ADJACENCY,
	GL_LINE_STRIP_ADJACENCY,
	GL_TRIANGLES_ADJACENCY,
	GL_TRIANGLE_STRIP_ADJACENCY,
	GL_PATCHES
};

const char* KEYBOARD_KEYCODES_TEXT = R"(// KeyboardTexture: 256x3 
//  row 0: current state of the key
//  row 1: keypress event
//  row 2: toggle
// example:
//  texelFetch(kbTexture, ivec2(KEY_UP, 0), 0)  -> GLSL
//  kbTexture.Load(int3(KEY_UP, 0, 0))       -> HLSL

// KEYCODES //
const int KEY_BACKSPACE = 8;
const int KEY_TAB = 9;
const int KEY_RETURN = 13;
const int KEY_PAUSE = 19;
const int KEY_CAPSLOCK = 20;
const int KEY_ESCAPE = 27; 
const int KEY_PAGEUP = 33;
const int KEY_SPACE = 32;
const int KEY_PAGEDOWN = 34; 
const int KEY_END = 35; 
const int KEY_HOME = 36; 
const int KEY_LEFT = 37; 
const int KEY_UP = 38; 
const int KEY_RIGHT = 39; 
const int KEY_DOWN = 40; 
const int KEY_PRINTSCREEN = 44;
const int KEY_INSERT = 45;
const int KEY_DELETE = 46;
const int KEY_0 = 48;
const int KEY_1 = 49;
const int KEY_2 = 50;
const int KEY_3 = 51; 
const int KEY_4 = 52;
const int KEY_5 = 53;
const int KEY_6 = 54;
const int KEY_7 = 55;
const int KEY_8 = 56;
const int KEY_9 = 57;
const int KEY_a = 65;
const int KEY_b = 66;
const int KEY_c = 67;
const int KEY_d = 68; 
const int KEY_e = 69; 
const int KEY_f = 70; 
const int KEY_g = 71; 
const int KEY_h = 72; 
const int KEY_i = 73; 
const int KEY_j = 74; 
const int KEY_k = 75; 
const int KEY_l = 76; 
const int KEY_m = 77; 
const int KEY_n = 78; 
const int KEY_o = 79; 
const int KEY_p = 80; 
const int KEY_q = 81; 
const int KEY_r = 82; 
const int KEY_s = 83; 
const int KEY_t = 84; 
const int KEY_u = 85; 
const int KEY_v = 86; 
const int KEY_w = 87; 
const int KEY_x = 88; 
const int KEY_y = 89; 
const int KEY_z = 90; 
const int KEY_SELECT = 93; 
const int KEY_KP_0 = 96; 
const int KEY_KP_1 = 97; 
const int KEY_KP_2 = 98; 
const int KEY_KP_3 = 99; 
const int KEY_KP_4 = 100; 
const int KEY_KP_5 = 101; 
const int KEY_KP_6 = 102; 
const int KEY_KP_7 = 103; 
const int KEY_KP_8 = 104; 
const int KEY_KP_9 = 105; 
const int KEY_KP_MULTIPLY = 106; 
const int KEY_KP_PLUS = 107; 
const int KEY_KP_MINUS = 109; 
const int KEY_KP_DECIMAL = 110;
const int KEY_KP_DIVIDE = 111;
const int KEY_F1 = 112; 
const int KEY_F2 = 113; 
const int KEY_F3 = 114; 
const int KEY_F4 = 115; 
const int KEY_F5 = 116; 
const int KEY_F6 = 117; 
const int KEY_F7 = 118;
const int KEY_F8 = 119;
const int KEY_F9 = 120;
const int KEY_F10 = 121;
const int KEY_F11 = 122;
const int KEY_F12 = 123;
const int KEY_NUMLOCK = 144;
const int KEY_SCROLLLOCK = 145;
const int KEY_SEMICOLON = 186;
const int KEY_EQUALS = 187;
const int KEY_COMMA = 188;
const int KEY_MINUS = 189;
const int KEY_PERIOD = 190; 
const int KEY_SLASH = 191; 
const int KEY_LEFTBRACKET = 219;
const int KEY_BACKSLASH = 220;
const int KEY_RIGHTBRACKET = 221;
const int KEY_QUOTE = 222;
const int BUTTON_LEFT = 245;
const int BUTTON_MIDDLE = 246;
const int BUTTON_RIGHT = 247;
const int MOUSE_SCROLL_UP = 250;	// row 0: not used; row 1: 0xFF if user is scrolling up; row 2: value that gets increased by 1 with each scroll
const int MOUSE_SCROLL_DOWN = 251;)";

namespace ed {
	namespace gl {
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
		const char* String::TextureMinFilter(unsigned int val)
		{
			int len = (sizeof(TEXTURE_MIN_FILTER_VALUES) / sizeof(*TEXTURE_MIN_FILTER_VALUES));

			for (int i = 0; i < len; i++)
				if (val == TEXTURE_MIN_FILTER_VALUES[i])
					return TEXTURE_MIN_FILTER_NAMES[i];
			return TEXTURE_MIN_FILTER_NAMES[4];
		}
		const char* String::TextureMagFilter(unsigned int val)
		{
			int len = (sizeof(TEXTURE_MAG_FILTER_VALUES) / sizeof(*TEXTURE_MAG_FILTER_VALUES));

			for (int i = 0; i < len; i++)
				if (val == TEXTURE_MAG_FILTER_VALUES[i])
					return TEXTURE_MAG_FILTER_NAMES[i];
			return TEXTURE_MAG_FILTER_NAMES[1];
		}
		const char* String::TextureWrap(unsigned int val)
		{
			int len = (sizeof(TEXTURE_WRAP_VALUES) / sizeof(*TEXTURE_WRAP_VALUES));

			for (int i = 0; i < len; i++)
				if (val == TEXTURE_WRAP_VALUES[i])
					return TEXTURE_WRAP_NAMES[i];
			return TEXTURE_WRAP_NAMES[1];
		}
	}
}