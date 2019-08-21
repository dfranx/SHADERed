cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matGeo;
};

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;

	vout.Position = mul(mul(float4(vin.Position, 1.0f), matGeo), matVP);
	vout.Color = 1;

	return vout;
}