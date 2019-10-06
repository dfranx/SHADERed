#pragma once

#define PIPELINE_ITEM_NAME_LENGTH 64
// #define SEMANTIC_LENGTH 32
#define VARIABLE_NAME_LENGTH 256
#define MAX_RENDER_TEXTURES 16

#define MODEL_GROUP_NAME_LENGTH 64

#define SDL_GLSL_VERSION "#version 330"
#define IMGUI_INI_FILE "data/workspace.dat"

// TODO: maybe dont use MAX_PATH but rather custom define?
#ifdef MAX_PATH
	#undef MAX_PATH
#endif

#ifdef _WIN32
	#define MAX_PATH 260
#else
	#define MAX_PATH 512
#endif