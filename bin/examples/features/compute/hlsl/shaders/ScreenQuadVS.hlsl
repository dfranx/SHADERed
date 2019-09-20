struct VOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VOutput main(uint id:SV_VERTEXID)
{
    VOutput output;

    // generate clip space position and texture coordinates
	output.uv = float2((id << 1) & 2, id & 2);
	output.position = float4(output.uv * float2(2.0f, 2.0f) + float2(-1.0f, -1.0f), 0.0f, 1.0f);    

    return output;
}
