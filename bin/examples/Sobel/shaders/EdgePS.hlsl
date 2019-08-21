cbuffer cbPerFrame : register(b0)
{
	float2 wndSize;
};

struct PSInput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD;
};

Texture2D tex : register(t0);
SamplerState smp : register(s0);

// One pixel offset
static const float offX = 1.0 / wndSize.x;
static const float offY = 1.0 / wndSize.y;

float4 main(PSInput pin) : SV_TARGET {
	pin.UV.y = 1 - pin.UV.y;

	float s00 = tex.Sample(smp, pin.UV + float2(-offX, -offY)).a;
	float s01 = tex.Sample(smp, pin.UV + float2( 0,   -offY)).a;
	float s02 = tex.Sample(smp, pin.UV + float2( offX, -offY)).a;

	float s10 = tex.Sample(smp, pin.UV + float2(-offX,  0)).a;
	float s12 = tex.Sample(smp, pin.UV + float2( offX,  0)).a;
	
	float s20 = tex.Sample(smp, pin.UV + float2(-offX,  offY)).a;
	float s21 = tex.Sample(smp, pin.UV + float2( 0,    offY)).a;
	float s22 = tex.Sample(smp, pin.UV + float2( offX,  offY)).a;
	
	float sobelX = s00 + 2 * s10 + s20 - s02 - 2 * s12 - s22;
	float sobelY = s00 + 2 * s01 + s02 - s20 - 2 * s21 - s22;

	float edgeSqr = (sobelX * sobelX + sobelY * sobelY);
	float4 clr = (edgeSqr > 0.07 * 0.07);
	clr.a = 1;
	return clr;
}