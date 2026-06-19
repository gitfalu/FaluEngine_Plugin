cbuffer SkySettingsCB : register(b1)
{
    float4 topColor;
    float4 bottomColor;
    float4 horizonColor;
    int useTexture;
    float exposure;
    float2 _pad;
};

Texture2D gSkyTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 position : SV_Position;
    float3 localPos : TEXCOORD0;
};

float2 dirToUV(float3 dir)
{
    float3 d = normalize(dir);
    float u = 0.5f * atan2(d.z, d.x) / (2.0f * 3.1415926535f);
    float v = 0.5f - asin(d.y) / 3.1415926535f;
    return float2(u, v);
}

float4 PS(PSInput input) : SV_TARGET
{
    float3 dir = normalize(input.localPos);
    
    if(useTexture != 0)
    {
        float2 uv = dirToUV(dir);
        float4 texColor = gSkyTexture.Sample(gSampler, uv);
        return float4(texColor.rgb * exposure, 1.0f);
    }
    
    float t = dir.y * 0.5f + 0.5f;
    
    float3 color;
    if(t > 0.5f)
    {
        color = lerp(horizonColor.rgb, topColor.rgb, (t - 0.5f) * 2.0f);
    }
    else
    {
        color = lerp(bottomColor.rgb, horizonColor.rgb, t * 2.0f);
    }
    
    return float4(color, 1.0f);
}