struct PSInput
{
	float4 Position : SV_POSITION;
	float4 PosInWorld : TEXCOORD0;
	float4 Color : COLOR;
	float3 Normal : NORMAL;
};

struct PSOutput
{
	float4 Color0 : SV_TARGET0;
	float4 Color1 : SV_TARGET1;
	float4 Color2 : SV_TARGET2;
};

PSOutput main(PSInput pin)
{
	PSOutput pout;
	pout.Color0 = pin.PosInWorld;
	pout.Color1 = float4(pin.Normal, 1);
	pout.Color2 = pin.Color;
	
	return pout;
}