#version 330

layout (location = 0) in vec2 pos;

void main() {
   gl_Position = vec4(pos, 0.0f, 1);
}
