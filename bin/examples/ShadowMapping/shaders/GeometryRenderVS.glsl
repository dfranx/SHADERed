#version 330

/* https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping */

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec4 FragPosLightSpace;

uniform mat4 matProj;
uniform mat4 matCamView;
uniform mat4 matGeo;
uniform mat4 matLightView;

void main()
{    
    FragPos = vec3(matGeo * vec4(aPos, 1.0));
    Normal = transpose(inverse(mat3(matGeo))) * aNormal;
    TexCoords = aTexCoords;
    FragPosLightSpace = matProj * matLightView * vec4(FragPos, 1.0);
    gl_Position = matProj * matCamView * matGeo * vec4(aPos, 1.0);
}