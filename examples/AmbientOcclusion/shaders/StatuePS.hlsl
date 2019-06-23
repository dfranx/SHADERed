struct PSInput
{
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
	float2 UV : TEXCOORD;
};

SamplerState smp : register(s0);
Texture2D ao : register(t0);
TextureCube diffuse : register(t1);

float4 main(PSInput pin) : SV_TARGET
{
	float amb = ao.Sample(smp, pin.UV);
	float dif = diffuse.Sample(smp, pin.Normal);
	float4 ret = amb*dif;
	ret.a = 1;
	return ret;
}