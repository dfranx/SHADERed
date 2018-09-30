cbuffer cbPerFrame : register(b0)
{
	matrix matWVP;
	matrix matWorld;
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

static const float3 LightPos = float3(10, 10, 10);
static const float4 LightDiffuse = float4(0.5f, 0.5f, 0.5f, 1);
static const float4 mDiffuse = float4(1,1,1,1);
static const float3 mAmbient = float4(0.2f, 0.2f, 0.2f, 0.0f);

VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;

	vout.Position = mul(float4(vin.Position, 1.0f), matWVP);
	vout.Color = float4(1,1,1,1);

	return vout;
}