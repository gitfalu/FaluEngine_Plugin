cbuffer TransformCB : register(b0)
{
    matrix mvp;
};


struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

PSInput VS(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), mvp);
    output.color = input.color;
    output.uv = input.uv;
    return output;
}