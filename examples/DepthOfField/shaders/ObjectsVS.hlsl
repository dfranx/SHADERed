cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matGeo;
	float4x4 matView;
};

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
	float Depth : TEXCOORD1;
};
		
VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;
	
	vout.Position = mul(mul(float4(vin.Position, 1), matGeo), matVP);
	vout.Depth = mul(mul(float4(vin.Position,1),matGeo), matView).z;
	vout.Normal = vin.Normal;
	return vout;
}