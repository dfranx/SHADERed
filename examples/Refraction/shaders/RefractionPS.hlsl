cbuffer cbData : register(b0)
{
	float factor;
	float darkness;
};

struct PSInput
{
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
	float3 ViewVec : TEXCOORD;
};

TextureCube tex : register(t0);
SamplerState smp : register(s0);

float4 main(PSInput pin) : SV_TARGET
{
	float3 n = pin.Normal;
	float3 rn = refract(pin.ViewVec, n, factor);
	rn.xy = -rn.xy;
	
	float4 r = tex.Sample(smp, rn);
	return r*darkness;
}