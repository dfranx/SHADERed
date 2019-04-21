cbuffer cbStatic : register(b0)
{
	matrix matVP;
	matrix matGeo;
	matrix matView;
};

struct VINPUT
{
	float3 Pos : POSITION;
	float3 Normal : NORMAL;
};

struct VOUTPUT
{
	float4 Pos : SV_POSITION;
	float4 Normal : NORMAL;
};

VOUTPUT main(VINPUT vin)
{
	VOUTPUT vout;

	vout.Pos = mul(mul(float4(vin.Pos, 1.0f), matGeo), matVP);
	vout.Normal = mul(float4(vin.Normal, 1.0f), matView);
	
	vout.Normal = normalize(vout.Normal * 0.5f + 0.5f);
	
	return vout;
}