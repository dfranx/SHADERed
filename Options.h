#pragma once

#define PIPELINE_ITEM_NAME_LENGTH 64
#define SEMANTIC_LENGTH 16
#define VARIABLE_NAME_LENGTH 32
#define MAX_RENDER_TEXTURES 16

#define MODEL_GROUP_NAME_LENGTH 64

#define SDL_GLSL_VERSION "#version 330"
#define IMGUI_INI_FILE "data/workspace.dat"
#define IMGUI_ERROR_COLOR ImVec4(1.0f, 0.17f, 0.13f, 1.0f)
#define IMGUI_WARNING_COLOR ImVec4(1.0f, 0.8f, 0.0f, 1.0f)
#define IMGUI_MESSAGE_COLOR ImVec4(0.106f, 0.631f, 0.886f, 1.0f)

// TODO: maybe dont use MAX_PATH but rather custom define?
#ifdef MAX_PATH
	#undef MAX_PATH
#endif

#ifdef _WIN32
	#define MAX_PATH 260
#else
	#define MAX_PATH 512
#endif