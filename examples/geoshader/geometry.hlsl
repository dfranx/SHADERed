cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matGeo;
	float scale;
};

struct GSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
};

struct GSOutput
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

[maxvertexcount(3)]
void main(
	triangle GSInput input[3],
	inout TriangleStream< GSOutput > output
)
{
	matrix matWVP = mul(matGeo, matVP);

	float3 nc = normalize((input[0].Normal + input[1].Normal + input[2].Normal) / 3);

	for (int i = 0; i < 3; i++) {
		float3 p = input[i].Position;

		float4 newPos = float4(p+scale*nc, 1);

		GSOutput gout;
		gout.Position = mul(newPos, matWVP);
		gout.Color = abs(float4(input[i].Normal, 1.0f));
		output.Append(gout);
	}
	//output.RestartStrip();
}