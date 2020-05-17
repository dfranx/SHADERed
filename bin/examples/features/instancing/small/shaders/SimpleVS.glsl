#version 450  

uniform mat4 matVP;
uniform mat4 matGeo;

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;

layout (binding = 0) buffer ubo {
	vec4 test[15];
};

out vec4 color;

void main() {
   color = vec4(abs(normal), 1.0);
   gl_Position = matVP * matGeo * vec4(pos + test[gl_InstanceID].xyz, 1);
}
