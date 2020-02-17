Texture2D<float4> Tex0S : register(t0);
SamplerState Sampler0 : register(s0);

struct VS_OUTPUT
{
	float4 position  : SV_POSITION;
	noperspective float2 uv: TEXCOORD;
};

float4 main( VS_OUTPUT In) : SV_TARGET
{
	return Tex0S.Sample(Sampler0, In.uv);
}
