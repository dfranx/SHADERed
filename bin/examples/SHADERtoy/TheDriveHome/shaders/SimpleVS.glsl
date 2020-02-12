#version 330

uniform mat4 matVP;
uniform mat4 matGeo;

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;

out vec4 coord;

void main() {
   gl_Position = vec4(pos, 1) * matVP * matGeo;
}
