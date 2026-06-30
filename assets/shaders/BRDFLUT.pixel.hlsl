static const float PI = 3.14159265359f;

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float radicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    
    return float(bits) * 2.3283064365386963e-10f;
}

float2 hammersley(uint i , uint N)
{
    return float2(float(i) / float(N), radicalInverseVdC(i));
}

float3 importanceSampleGGX(float2 Xi,float3 N,float roughness)
{
    float a = roughness * roughness;
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
    
    return normalize(right * H.x + up * H.y * H.z);
}

float geometrySchlickGGX_IBL(float NdotV,float NdotL,float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float2 integrateBRDF(float NdotV ,float roughness)
{
    float3 V;
    V.x = sqrt(1.0f - NdotV * NdotV);
    V.y = 0.0f;
    V.z = NdotV;
    
    float A = 0.0f;
    float B = 0.0f;
    
    float3 N = float3(0, 0, 1);
    
    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT;++i)
    {
        float2 Xi = hammersley(i, SAMPLE_COUNT);
        float3 H = importanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);
        
        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V,H), 0.0f);
        
        if (NdotL > 0.0f)
        {
            float G = geometrySchlickGGX_IBL(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotH);
            float Fc = pow(1.0f - VdotH, 5.0f);
            
            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return float2(A, B);
}

float4 PS(PSInput pin) : SV_TARGET
{
    float2 integratedBRDF = integrateBRDF(pin.uv.x, pin.uv.y);
    return float4(integratedBRDF, 0.0f, 1.0f);
}