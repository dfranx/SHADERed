#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D smp;

void main() {
   outColor = texture(smp, uv);
}