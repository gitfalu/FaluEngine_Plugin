struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput VS(uint vertexID : SV_VertexID)
{
    VSOutput vout;
    
    
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    vout.uv = uv;
    vout.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return vout;
}