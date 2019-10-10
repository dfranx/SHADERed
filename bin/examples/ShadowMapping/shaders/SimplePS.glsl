#version 330

out vec4 outColor;

void main() {
   outColor = vec4(gl_FragCoord.z);
}