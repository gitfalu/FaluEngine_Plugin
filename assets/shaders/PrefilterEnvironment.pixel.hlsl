cbuffer PrefilterCB : register(b2)
{
    float roughness;
    float3 _pad;
};

TextureCube gEnvironmentMap : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 position : SV_Position;
    float3 localPos : TEXCOORD0;
};

static const float PI = 3.14159265259f;

float3 importanceSampleGGX(float2 Xi, float3 N, float rough)
{
    float a = rough * rough;
    
    float phi = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
    
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    float3 up = abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 right = normalize(cross(up, N));
    up = cross(N, right);
    
    return normalize(right * H.x + up * H.y + N * H.z);
}

float radicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

float2 hammersley(uint i,uint N)
{
    return float2(float(i) / float(N), radicalInverseVdC(i));
}

float4 PS(PSInput pin) : SV_TARGET
{
    float3 N = normalize(pin.localPos);
    float3 V = N;
    
    float3 prefileteredColor = float3(0, 0, 0);
    float totalWeight = 0.0f;
    
    const uint SAMPLE_COUNT = 64u;
    
    for (uint i = 0u; i < SAMPLE_COUNT;++i)
    {
        float2 Xi = hammersley(i, SAMPLE_COUNT);
        float3 H = importanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);
        
        float NdotL = dot(N, L);
        if(NdotL > 0.0f)
        {
            prefileteredColor += gEnvironmentMap.Sample(gSampler, L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    
    prefileteredColor = prefileteredColor / max(totalWeight, 0.0001f);
    return float4(prefileteredColor, 1.0f);
}