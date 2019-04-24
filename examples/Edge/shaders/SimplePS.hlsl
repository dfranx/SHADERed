struct PSInput
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
	float3 Normal : NORMAL;
};

struct PSOutput
{
	float4 Color : SV_TARGET0;
	float4 Normal : SV_TARGET1;
};

PSOutput main(PSInput pin)
{
	PSOutput pout;
	
	pout.Color = pin.Color;
	pout.Normal = float4(pin.Normal,1);
	
	return pout;
}