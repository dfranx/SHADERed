cbuffer cbPerFrame : register(b0)
{
	float4x4 matVP;
	float4x4 matGeo;
	float fTime;
	float fTimeScale;
};

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float2 UV : TEXCOORD;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
	float2 UV : TEXCOORD;
};
		
VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;
	
	float4 n = float4(vin.Normal,1);
	float4 rotN = n;
	rotN.x = dot(n.xyz, float3(cos(fTimeScale*fTime), 0, -sin(fTimeScale*fTime)));
	rotN.z = dot(n.xyz, float3(sin(fTimeScale*fTime), 0,  cos(fTimeScale*fTime)));
	
	vout.Position = mul(mul(float4(vin.Position, 1), matGeo), matVP);
	vout.Normal = rotN;
	vout.UV = vin.UV;
	
	return vout;
}