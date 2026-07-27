cbuffer UIMaterialCB : register(b1)
{
    float4 color;
    int useTexture;
    float3 _pad;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 postion : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PS(PSInput pin) : SV_TARGET
{
    float4 result = color;
    if(useTexture != 0)
    {
        result *= gTexture.Sample(gSampler, pin.uv);
    }
    return result;
}