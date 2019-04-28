#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 0) uniform bufferVals {
    mat4 matVP;
    mat4 matGeo;
} cb;

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (location = 0) out vec4 outPos;
layout (location = 1) out vec4 outColor;
layout (location = 2) out vec2 outUV;

void main() {
	outPos = vec4(pos, 1) * cb.matGeo;
	gl_Position = outPos * cb.matVP;
	outUV = uv;
	
	outColor = vec4(abs(normal), 1.0);
}
