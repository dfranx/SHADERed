cbuffer cbPerFrame : register(b0)
{
	float2 viewSize;
	float2 vMaxCoC;
	float radiusScale;
};

#define NUM_TAPS 4

struct PSInput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD;
};

float2 poisson[8] = {  
  float2( 0.0,      0.0),
  float2( 0.527837,-0.085868),
  float2(-0.040088, 0.536087),
  float2(-0.670445,-0.179949),
  float2(-0.419418,-0.616039),
  float2( 0.440453,-0.639399),
  float2(-0.757088, 0.349334),
  float2( 0.574619, 0.685879)
};


Texture2D source : register(t0);
Texture2D sourceLow : register(t1);
SamplerState smp : register(s0);

float4 main(PSInput pin) : SV_TARGET
{
	float2 invViewSize = (1/viewSize);
	float4 cOut;
	float discRadius, discRadiusLow, centerDepth;
	
	float2 pixelSizeLow = 4.0 * invViewSize;

	cOut = source.Sample(smp, pin.UV);
	centerDepth = cOut.a;
	
	discRadius = abs(cOut.a * vMaxCoC.y - vMaxCoC.x);
	discRadiusLow = discRadius * radiusScale;
	
	cOut = 0;
	
	for(int t = 0; t < NUM_TAPS; t++) {
		float2 coordLow = pin.UV + (pixelSizeLow * poisson[t] *
                      discRadiusLow);
		float4 tapLow = sourceLow.Sample(smp, coordLow);

		float2 coordHigh = pin.UV + (invViewSize * poisson[t] *
                       discRadius);
		float4 tapHigh = source.Sample(smp, coordHigh);

		float tapBlur = abs(tapHigh.a * 2.0 - 1.0);
    
		float4 tap = lerp(tapHigh, tapLow, tapBlur);
		tap.a = (tap.a >= centerDepth) ? 1.0 : abs(tap.a * 2.0 - 1.0);

		cOut.rgb += tap.rgb * tap.a;
		cOut.a += tap.a;
	}
	
	return (cOut / cOut.a);
}