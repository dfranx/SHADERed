cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matGeo;
};

struct VSInput
{
	float3 Position : POSITION;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD;
};
		
VSOutput main(VSInput vin)
{
	float2 pos = sign(vin.Position.xy);
	
	float4 geoPos = mul(mul(float4(vin.Position, 1), matGeo),matVP);
	
	VSOutput vout = (VSOutput)0;
    vout.Position = float4(pos, geoPos.z, geoPos.w);
	
	vout.UV = 0.5 * pos + 0.5;
	vout.UV.y = 1.0 - vout.UV.y;
	
	return vout;
}