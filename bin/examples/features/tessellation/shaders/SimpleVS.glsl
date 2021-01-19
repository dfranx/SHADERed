#version 330

uniform mat4 matGeo;
uniform mat4 matVP;

layout (location = 0) in vec3 pos;

out vec3 posCS;

void main() {
	gl_Position = matVP * matGeo * vec4(pos, 1);
	posCS = (matGeo * vec4(pos, 1)).xyz; 
}
