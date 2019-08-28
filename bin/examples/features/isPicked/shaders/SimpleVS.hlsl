cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matGeo;
	int isPicked;
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
};
		
VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;
	
	vout.Position = mul(mul(float4(vin.Position, 1), matGeo), matVP);
	vout.Color = float4(1.0f, isPicked, 0.0f, 1);
	
	return vout;
}