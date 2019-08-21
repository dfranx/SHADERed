cbuffer cbPerFrame : register(b0)
{
	float2 mPos;
};

struct PSInput
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
	float2 UV : TEXCOORD;
};

Texture2D tex : register(t0);
SamplerState smp : register(s0);

static const float radius = 0.2f;

float4 main(PSInput pin) : SV_TARGET
{
	float4 pixel = tex.Sample(smp, pin.UV);

	bool isIn = length(float2(pin.UV.x - mPos.x, pin.UV.y - mPos.y)) < radius;
	float sum = ((pixel.x+pixel.y+pixel.z)/3) * isIn;

	return float4(sum, sum, sum, 1);
}