struct PSInput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD;
};

Texture2D tex : register(t0);
SamplerState smp : register(s0);

float4 main(PSInput pin) : SV_TARGET
{
	return tex.Sample(smp, pin.UV);
}