#version 330

uniform mat4 matVP;
uniform mat4 matGeo;

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 offset;
layout (location = 4) in vec3 iColor;

out vec4 color;

void main() {
   color = vec4(iColor, 1.0);
   gl_Position = matVP * matGeo * vec4(pos + offset, 1);
}
