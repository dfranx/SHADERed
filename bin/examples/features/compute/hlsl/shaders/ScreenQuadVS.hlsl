struct VOutput
{
    float4 position : SV_POSITION;
    noperspective float2 uv : TEXCOORD0;
};

struct VInput
{
	float2 Position : POSITION;
	float2 UV : TEXCOORD;
};

VOutput main(VInput vin)
{
    VOutput output;

	output.uv = vin.UV;
	output.position = float4(vin.Position, 0.0f, 1.0f);
	
    return output;
}
