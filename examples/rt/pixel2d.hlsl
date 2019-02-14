struct PSInput
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
	float2 UV : TEXCOORD;
};

Texture2D tex : register(t0);
SamplerState smp : register(s0);

float4 main(PSInput pin) : SV_TARGET
{
	float4 pixel = tex.Sample(smp, pin.UV);
	float sum = (pixel.x+pixel.y+pixel.z)/3;
	return float4(sum, sum, sum, 1);
}