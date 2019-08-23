#version 330

uniform mat4 matVP;
uniform mat4 matGeo;

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

out vec4 outPos;
out vec4 outColor;
out vec2 outUV;

void main() {
	outPos = matGeo * vec4(pos, 1);
	gl_Position = matVP * outPos;
	outUV = uv;
	
	outColor = vec4(abs(normal), 1.0);
}
