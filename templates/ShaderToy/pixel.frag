#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec4 coord;

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform bufferVals {
	vec2 iResolution;
	vec2 iMouse;
	float iTime;
	float iTimeDelta;
};

void main( void ) {
    vec2 uv = gl_FragCoord.xy / iResolution.xy; // 0 <> 1
    
	outColor = vec4(uv * sin(iTime), cos(iTime), 1.);
}