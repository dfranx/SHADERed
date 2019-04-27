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
Texture2D dof2 : register(t2);
SamplerState smp : register(s0);

float4 main(PSInput pin) : SV_TARGET
{
  float4 cOut;
  float discRadius, discRadiusLow, centerDepth;

  // pixel size of low resolution image
	float2 invVSize = 1/(viewSize);
  float2 pixelSizeLow = 4.0 * invVSize;

  cOut = source.Sample(smp, pin.UV);   // fetch center tap
  centerDepth = cOut.a;              // save its depth

  // convert depth into blur radius in pixels
  discRadius = abs(cOut.a * vMaxCoC.y - vMaxCoC.x);

  // compute disc radius on low-res image
  discRadiusLow = discRadius * radiusScale;
  
  // reuse cOut as an accumulator
  cOut = 0;

  for(int t = 4; t < 4+NUM_TAPS; t++)
  {
    // fetch low-res tap
    float2 coordLow = pin.UV + (pixelSizeLow * poisson[t] *
                      discRadiusLow);
    float4 tapLow = sourceLow.Sample(smp, coordLow);

    // fetch high-res tap
    float2 coordHigh = pin.UV + (invVSize * poisson[t] *
                       discRadius);
    float4 tapHigh = source.Sample(smp, coordHigh);

    // put tap blurriness into [0, 1] range
    float tapBlur = abs(tapHigh.a * 2.0 - 1.0);
    
    // mix low- and hi-res taps based on tap blurriness
    float4 tap = lerp(tapHigh, tapLow, tapBlur);

    // apply leaking reduction: lower weight for taps that are
    // closer than the center tap and in focus
    tap.a = (tap.a >= centerDepth) ? 1.0 : abs(tap.a * 2.0 - 1.0);

    // accumulate
    cOut.rgb += tap.rgb * tap.a;
    cOut.a += tap.a;
  }

  // normalize and return result
  cOut = cOut / cOut.a;
  
  // alpha of 0.5 so that alpha blending averages results with
  // previous DoF pass
	float alpha = 0.5;
	
	float4 px = dof2.Sample(smp, pin.UV) * (1-alpha);
	
  return cOut*alpha + px;
}