RWTexture2D<float4> img_output : register(u0);

cbuffer varBuffer : register(b0) {
	float roll;
}

[numthreads(16, 16, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID, uint3 groupID : SV_GroupID )
{
    float localCoef = length(float2(int2(groupThreadID.xy)-8)/8.0);
    float globalCoef = sin(float(groupID.x+groupID.y)*1 + roll)*0.5f;
	
    float4 px = float4(1.0-globalCoef*localCoef, 0.0f, 0.0f, 1.0f);
    img_output[dispatchThreadID.xy] = px;
}
