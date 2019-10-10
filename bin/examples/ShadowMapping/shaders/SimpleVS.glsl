#version 330

uniform mat4 matView;
uniform mat4 matProj;
uniform mat4 matGeo;

layout (location = 0) in vec3 pos;

out vec4 color;

void main() {
   gl_Position = matProj * matView * matGeo * vec4(pos, 1);
}
