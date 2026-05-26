cbuffer MaterialCB : register(b1)
{
    int useTexture;
    float3 _pad;
};

Texture2D glTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

float4 PS(PSInput input) : SV_TARGET
{
    if(useTexture)
    {
        float4 texColor = glTexture.Sample(gSampler, input.uv);
        return texColor * input.color;
    }
    
    return input.color;
}