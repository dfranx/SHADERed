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
	float Depth : DEPTH;
};
		
VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;
	
	vout.Position = mul(mul(float4(vin.Position, 1), matGeo), matVP);
	vout.Depth = vout.Position.z/vout.Position.w;
	
	return vout;
}