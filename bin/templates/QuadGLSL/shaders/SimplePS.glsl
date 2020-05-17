#version 330

uniform vec2 uResolution;
uniform float uTime;

out vec4 outColor;

void main()
{
    vec2 uv = gl_FragCoord.xy/uResolution;
    outColor = vec4(0.5 + 0.5*cos(uTime+uv.yxy),1.0);
}