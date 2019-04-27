cbuffer cbPerFrame : register(b0)
{
	float4x4 matView;
	float4x4 matVP;
	float4x4 matGeo;
	float3 lightDir;
};

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD;
};
		
VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;
	vout.Position = mul(mul(float4(vin.Position, 1), matGeo), matVP);
	
	float3 normal = mul(vin.Normal, matView);

	float diffuse = saturate(dot(-lightDir,normal));
	vout.UV.x = diffuse;
	vout.UV.y = 0.0f;

	return vout;
}