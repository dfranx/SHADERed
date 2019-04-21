cbuffer cbPerFrame : register(b0)
{
	float4x4 matP;
	float4x4 matGeo;
	float fTime;
	float fTimeScale;
};

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
};
		
VSOutput main(VSInput vin)
{
	VSOutput vout = (VSOutput)0;
	
	float4 pos = float4(vin.Position, 1);
	
   float4 rotPos = pos;
   rotPos.x = dot(pos.xyz, float3(cos(fTimeScale*fTime), 0, -sin(fTimeScale*fTime)));
   rotPos.z = dot(pos.xyz, float3(sin(fTimeScale*fTime), 0,  cos(fTimeScale*fTime)));
	
	vout.Position = mul(mul(rotPos, matGeo), matP);
	vout.Normal = -vin.Normal;
	
	return vout;
}