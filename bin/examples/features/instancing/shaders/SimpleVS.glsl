#version 330

uniform mat4 matVP;
uniform mat4 matGeo;

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;

layout (std140) uniform ubo {
	vec4 test;
};
layout (std140) uniform tester {
	vec4 test2;
};

out vec4 color;

void main() {
   color = test2;//vec4(abs(normal), 1.0);
   gl_Position = matVP * matGeo * vec4(pos, 1);
}
