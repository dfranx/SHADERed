#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aTangent;
layout (location = 3) in vec3 aBitangent;

out VS_OUT {
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    mat3 normalMatrix = mat3(transpose(inverse(view * model)));
    vs_out.normal = vec3(projection * vec4(normalMatrix * aNormal, 0.0));
    vs_out.tangent = vec3(projection * vec4(normalMatrix * aTangent, 0.0));
	vs_out.bitangent = normalize(aBitangent);
    gl_Position = projection * view * model * vec4(aPos, 1.0); 
}