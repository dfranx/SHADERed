struct PSInput
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

float4 main(PSInput pin) : SV_TARGET
{
	float4 ret = pin.Color;
	ret.a = 0.5f;
	return ret;
}

