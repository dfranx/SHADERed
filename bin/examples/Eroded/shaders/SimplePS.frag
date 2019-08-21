#version 330

in vec4 outPos;
in vec4 outColor;
in vec2 outUV;

out vec4 fragColor;

uniform float eFactor;
uniform float iFactor1;
uniform float iFactor2;

uniform sampler2D Noise2d;

void main() {
    vec3 offset = vec3(-eFactor, -eFactor + 0.06, -eFactor * 0.92);
    vec3 color;   
    
    // Compute noise
    vec3 noiseCoord = outPos.xyz + offset;
    vec4 noiseVec   = texture(Noise2d, outUV);
    float intensity = abs(noiseVec[0] - 0.25) +
                      abs(noiseVec[1] - 0.125) +
                      abs(noiseVec[2] - 0.0625) +
                      abs(noiseVec[3] - 0.03125);
    
    // continue noise evaluation                  
    intensity = iFactor1 * (noiseVec.x + noiseVec.y + noiseVec.z + noiseVec.w);
    intensity = iFactor2 * abs(2.0 * intensity - 1.0);
    intensity = clamp(intensity, 0.0, 1.0);

    // discard pixels in a psuedo-random fashion (noise)
    if (intensity < fract(0.5-offset.x-offset.y-offset.z)) {
		discard;
	}

    fragColor = outColor;
}