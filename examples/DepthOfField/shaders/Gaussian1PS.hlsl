cbuffer cbPerFrame : register(b0)
{
	float2 viewSize;
};

struct PSInput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD;
};

Texture2D tex : register(t0);
SamplerState smp : register(s0);

float4 main(PSInput pin) : SV_TARGET
{
   float2 texCoordSample = 0;
   float4 cOut;
   
   float2 pixelSize = 4.0 / viewSize;


   cOut = 0.5 * tex.Sample(smp, pin.UV);
    
   texCoordSample.x = pin.UV.x;
   texCoordSample.y = pin.UV.y + pixelSize.y;
   cOut += 0.25 * tex.Sample(smp, texCoordSample);
   
   texCoordSample.y = pin.UV.y - pixelSize.y;
   cOut += 0.25 * tex.Sample(smp, texCoordSample);
      
   return tex.Sample(smp, pin.UV);
}