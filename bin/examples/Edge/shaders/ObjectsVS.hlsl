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
	float3 Normal : NORMAL;
};
		
VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;
	
	vout.Position = mul(mul(float4(vin.Position, 1), matGeo), matVP);
	vout.Color = float4(1,0,0, 1);
	vout.Normal = vin.Normal;
	
	return vout;
}