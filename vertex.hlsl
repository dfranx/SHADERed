cbuffer cbPerFrame : register(b0)
{
	matrix matWVP;
	matrix matWorld;
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
	float3 Normal : NORMAL;
};

static const float3 LightPos = float3(10, 10, 10);
static const float4 LightDiffuse = float4(0.5f, 0.5f, 0.5f, 1);
static const float4 mDiffuse = float4(1,1,1,1);
static const float3 mAmbient = float4(0.2f, 0.2f, 0.2f, 0.0f);

VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;

	vout.Position = mul(float4(vin.Position, 1.0f), matWVP);
	vout.Normal = vin.Normal;

	float3 worldPos = mul(vin.Position, (float3x3)matWorld);
	float3 worldNormal = normalize(mul(vin.Normal, (float3x3)matWorld));

	float3 lightVec = normalize(LightPos - worldPos);
	float diffuse = max(dot(worldNormal, lightVec), 0);

	vout.Color = float4(mAmbient, 0) + mDiffuse * LightDiffuse * diffuse;

	return vout;
}