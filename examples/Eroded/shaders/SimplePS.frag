#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 uv;

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform bufferVals {
    float eFactor;
    float iFactor1;
    float iFactor2;
} cb;

layout (binding = 0) uniform sampler2D Noise2d;

void main() {
    vec3 offset     = vec3(-cb.eFactor, -cb.eFactor + 0.06, -cb.eFactor * 0.92);
    vec3 color;   
    
    // Compute noise
    vec3 noiseCoord = pos.xyz + offset;
    vec4 noiseVec   = texture(Noise2d, uv);
    float intensity = abs(noiseVec[0] - 0.25) +
                      abs(noiseVec[1] - 0.125) +
                      abs(noiseVec[2] - 0.0625) +
                      abs(noiseVec[3] - 0.03125);
    
    // continue noise evaluation                  
    intensity = cb.iFactor1 * (noiseVec.x + noiseVec.y + noiseVec.z + noiseVec.w);
    intensity = cb.iFactor2 * abs(2.0 * intensity - 1.0);
    intensity = clamp(intensity, 0.0, 1.0);

    // discard pixels in a psuedo-random fashion (noise)
    if (intensity < fract(0.5-offset.x-offset.y-offset.z)) 	{
		discard;
	}

    outColor = inColor;
}