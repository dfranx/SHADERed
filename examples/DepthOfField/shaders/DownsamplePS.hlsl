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
   
   float2 pixelSize = 1.0 / viewSize;

   texCoordSample.x = pin.UV.x - pixelSize.x;
   texCoordSample.y = pin.UV.y + pixelSize.y;
   cOut = tex.Sample(smp, texCoordSample);
   
   texCoordSample.x = pin.UV.x + pixelSize.x;
   texCoordSample.y = pin.UV.y + pixelSize.y;
   cOut += tex.Sample(smp, texCoordSample);
   
   texCoordSample.x = pin.UV.x + pixelSize.x;
   texCoordSample.y = pin.UV.y - pixelSize.y;
   cOut += tex.Sample(smp, texCoordSample);
   
   texCoordSample.x = pin.UV.x - pixelSize.x;
   texCoordSample.y = pin.UV.y - pixelSize.y;
   cOut += tex.Sample(smp, texCoordSample);
      
   return cOut * 0.25;
}