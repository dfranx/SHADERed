struct PSInput
{
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;	
};

SamplerState smp : register(s0);
TextureCube tex : register(t0);

float4 main(PSInput pin) : SV_TARGET
{
	return tex.Sample(smp, pin.Normal);
}