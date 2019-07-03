cbuffer cbPerFrame : register(b0)
{
	float2 iResolution;
	float2 iMouse;
	float iTime;
	float iTimeDelta;
};

struct PSInput
{
	float4 Position : SV_POSITION;
};

float4 main(PSInput pin) : SV_TARGET
{
    float2 uv = pin.Position.xy / iResolution.xy; // 0 <> 1
    
    return float4(uv * sin(iTime), cos(iTime), 1.0f);
}