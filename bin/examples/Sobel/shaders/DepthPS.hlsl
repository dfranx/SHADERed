struct PSInput
{
	float Depth : DEPTH;
};

float4 main(PSInput pin) : SV_TARGET
{
	return pin.Depth;
}