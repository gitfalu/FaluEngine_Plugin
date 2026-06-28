cbuffer CubemapCB : register(b0)
{
    matrix viewProj;  
};

struct VSInput
{
    float3 position : POSITION;
};


struct PSInput
{
    float4 position : SV_Position;
    float3 localPos : TEXCOORD0;
};

PSInput VS(VSInput vin)
{
    PSInput vout;
    vout.localPos= vin.position;
    vout.position = mul(float4(vin.position, 1.0f), viewProj);
    return vout;
}