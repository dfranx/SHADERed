cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matGeo;
	float3 objColor;
};

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 PosInWorld : TEXCOORD0;
	float4 Color : COLOR;
	float3 Normal : NORMAL;
};
		
VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;
	
	vout.Position = mul(mul(float4(vin.Position, 1), matGeo), matVP);
	vout.PosInWorld = mul(float4(vin.Position, 1), matGeo);
	vout.Normal = mul(vin.Normal, (float3x3)matGeo);
	vout.Color = float4(objColor, 1);
	
	return vout;
}