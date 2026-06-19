cbuffer SkyCB : register(b0)
{
    matrix viewProj;
};

struct VSInput
{
    float3 position : POSITION;
};

struct PSInput
{
    float4 positoin : SV_Position;
    float3 localPos : TEXCOORD0;
};

PSInput VS(VSInput input)
{
    PSInput output;
    
    output.localPos = input.position;
    
    float4 pos = mul(float4(input.position, 1.0f), viewProj);
    output.positoin = pos.xyww;
    return output;
}