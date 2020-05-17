cbuffer vars : register(b0)
{
	float2 uResolution;
	float uTime;
};

float4 main(float4 fragCoord : SV_POSITION) : SV_TARGET
{
    float2 uv = fragCoord.xy/uResolution;
    return float4(0.5f + 0.5f*cos(uTime+uv.yxy), 1.0f);
}
