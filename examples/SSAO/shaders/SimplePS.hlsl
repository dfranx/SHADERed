struct PSInput
{
	float4 Position : SV_POSITION;
	float4 Normal : NORMAL;
};

float4 main(PSInput pin) : SV_TARGET
{
	return float4(pin.Normal.xyz * 0.5f + 0.5f, pin.Position.z / pin.Position.w);
}