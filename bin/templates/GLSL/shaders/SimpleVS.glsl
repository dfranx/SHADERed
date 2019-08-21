#version 330

uniform mat4 matVP;
uniform mat4 matGeo;

in vec3 pos;
in vec3 normal;

out vec4 color;

void main() {
   color = vec4(abs(normal), 1.0);
   gl_Position = matVP * matGeo * vec4(pos, 1);
}
