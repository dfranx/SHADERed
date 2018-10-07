cbuffer cbPerFrame : register(b0)
{
	matrix matWVP;
};

struct VSInput
{
	float3 Position : POSITION;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;

	vout.Position = mul(float4(vin.Position, 1.0f), matWVP);
	vout.Color = float4(1,1,1,1);

	return vout;
}