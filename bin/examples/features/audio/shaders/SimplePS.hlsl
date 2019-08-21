cbuffer cbPerFrame : register(b0)
{
	float2 iResolution;
	float2 iMouse;
	float iTime;
	float iTimeDelta;
};

struct PSInput
{
	float4 Position : SV_POSITION;
};

Texture2D tex : register(t0);
SamplerState smp : register(s0);

float4 main(PSInput pin) : SV_TARGET
{
    float2 uv = pin.Position.xy / iResolution.xy; // 0 <> 1

	
	// first row is frequency data (48Khz/4 in 512 texels, meaning 23 Hz per texel)
	float fft  = tex.Sample(smp, float2(uv.x,0)).x;
	
    // second row is the sound wave, one texel is one mono sample
    float wave = tex.Sample(smp, float2(uv.x,1)).x;
	
	// convert frequency to colors
	float3 col = float3( fft, 4.0*fft*(1.0-fft), 1.0-fft ) * fft;

    // add wave form on top	
	col += 1.0 -  smoothstep( 0.0, 0.15, abs(wave - uv.y) );
	
    return float4(col, 1.0f);
}