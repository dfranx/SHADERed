#version 330

uniform mat4 matVP;
uniform mat4 matGeo;

in vec3 pos;
in vec3 normal;

out vec4 coord;

void main() {
   gl_Position = coord = vec4(pos, 1) * matVP * matGeo;
}
