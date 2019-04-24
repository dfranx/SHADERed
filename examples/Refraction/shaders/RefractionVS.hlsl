cbuffer cbData : register(b0)
{
	matrix matGeo;
	matrix matVP;
	float4 ViewPos;
};

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Normal : NORMAL;
	float3 ViewVec : TEXCOORD;
};

VSOutput main(VSInput vin)
{
	float4 pos = mul(float4(vin.Position, 1), matGeo);
	
	VSOutput vout;
	vout.Position = mul(pos, matVP);
	vout.Normal = float4(vin.Normal, 1);
	vout.ViewVec = ViewPos - pos;
	
	return vout;
}