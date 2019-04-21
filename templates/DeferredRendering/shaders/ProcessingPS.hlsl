cbuffer cbPerFrame : register(b0)
{
	float3 lightPos;
};

struct PSInput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD;
};

SamplerState smp : register(s0);

Texture2D posTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D diffuseTex : register(t2);

float4 main(PSInput pin) : SV_TARGET
{
	float4 pos = posTex.Sample(smp,pin.UV);   
	clip((pos.w != 0) - 1);
	
	float3 normal = normalize(normalTex.Sample(smp, pin.UV).xyz);
	float3 toLight = normalize(lightPos - pos.xyz);
 
	float diffuse = saturate(dot(normal,toLight));
	
	float4 ret = diffuse * diffuseTex.Sample(smp, pin.UV);
	ret.a = 1;
	return ret;
}