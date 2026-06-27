#define MAX_LIGHTS 16
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2
#define PI 3.14159265359f

struct LightData
{
    float4 position;
    float4 direction;
    float4 color;
    int type;
    float range;
    float spotInner;
    float spotOuter;
};

cbuffer LightCB : register(b2)
{
    LightData lights[MAX_LIGHTS];
    int lightCount;
    float3 _pad0;
    float3 cameraPos;
    float _pad1;
    float4 ambientColor;
};

cbuffer ShadowSettingsCB : register(b3)
{
    matrix lightSpaceMatrix;
    int useShadow;
    int useSoftShadow;
    float shadowBias;
    float pcfRadius;
};

cbuffer MaterialCB : register(b1)
{
    float4 albedoColor;
    float metallic;
    float roughness;
    int useAlbedoMap;
    int useMetallicMap;
    int useNormalMap;
    int useAOMap;
    int useEmissiveMap;
    float emissiveStrength;
    float3 emissiveColor;
    float _matPad;
};

Texture2D gAlbedoMap : register(t0);
Texture2D gMetallicMap : register(t1);
Texture2D gNormalMap : register(t2);
Texture2D gAOMap : register(t3);
Texture2D gEmissiveMap : register(t4);
Texture2D gShadowMap : register(t5);
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float3 tangent : TEXCOORD3;
    float3 bitangent : TEXCOORD4;
};

//========= Cook-Torrance BRDF ============
/// GGX •ª•z
float distributionGGX(float3 N,float3 H,float3 roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
    
    return a2 / max(denom, 0.0000001f);
}

/// Smith's Geometry
float geometrySchlickGGX(float NdotV ,float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    
    float denom = NdotV * (1.0f - k) + k;
    return NdotV / max(denom, 0.0000001f);
}

float geometrySmith(float3 N,float3 V,float3 L , float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

/// Fresnel-Schlick
float3 fresnelSchlick(float cosTheta,float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

//====== Calclation Shadow ========
float calcShadow(float3 worldPos ,float bias)
{
    if (useShadow == 0) return 1.0f;
    
    float4 lightSpacePos = mul(float4(worldPos, 1.0f), lightSpaceMatrix);
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    float2 shadowUV = projCoords.xy * float2(0.5f, -0.5f) + 0.5f;
    
    if(shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f)
        return 1.0f;

    float currentDepth = projCoords.z - bias;
    
    if(useSoftShadow == 0)
        return gShadowMap.SampleCmpLevelZero(gShadowSampler, shadowUV, currentDepth);
    
    float shadow = 0.0f;
    float texelSize = 1.0f / 2048.0f;
    int samples = 0;
    for (int x = -1; x <= 1;++x)
    {
        for (int y = -1; y <= 1;++y)
        {
            float2 offset = float2(x, y) * texelSize * pcfRadius;
            shadow += gShadowMap.SampleCmpLevelZero(gShadowSampler, shadowUV + offset, currentDepth);
            ++samples;
        }
    }
    return shadow / float(samples);
}

float4 PS(PSInput pin) : SV_TARGET
{
    // Calc Albedo
    float4 albedo = albedoColor;
    if(useAlbedoMap != 0)
        albedo *= gAlbedoMap.Sample(gSampler, pin.uv);
    
    // Calc PBR
    float metal = metallic;
    float rough = roughness;
    if(useMetallicMap != 0)
    {
        float2 mr = gMetallicMap.Sample(gSampler, pin.uv).rg;
        metal = mr.r;
        rough = mr.g;
    }
    rough = clamp(rough, 0.04f, 1.0f);
    
    float ao = 1.0f;
    if(useAOMap != 0)
        ao = gAOMap.Sample(gSampler, pin.uv).r;
    
    float3 emissive = float3(0, 0, 0);
    if(useEmissiveMap != 0)
        emissive = gEmissiveMap.Sample(gSampler, pin.uv).rgb *
                    emissiveColor * emissiveStrength;
    else
        emissive = emissiveColor * emissiveStrength;
    
    // Get normal 
    float3 N = normalize(pin.normal);
    if(useNormalMap != 0)
    {
        float3 n = gNormalMap.Sample(gSampler, pin.uv).xyz * 2.0f - 1.0f;
        float3 T = normalize(pin.tangent);
        float3 B = normalize(pin.bitangent);
        float3x3 TBN = float3x3(T, B, N);
        N = normalize(mul(n, TBN));
    }
    
    float3 V = normalize(cameraPos - pin.worldPos);
    
    // 
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, metal);
    
    float3 Lo = float3(0, 0, 0);
    
    for (int i = 0; i < lightCount;++i)
    {
        LightData light = lights[i];
        float3 lightColorRaw = light.color.rgb;
        float intensity = light.color.w;
        float3 L = float3(0, 0, 0);
        float attenuation = 1.0f;
        
        if(light.type == LIGHT_DIRECTIONAL)
        {
            L = normalize(-light.direction.xyz);
        }
        else if(light.type == LIGHT_POINT)
        {
            float3 toLight = light.position.xyz - pin.worldPos;
            float dist = length(toLight);
            if (dist > light.range)
                continue;
            L = normalize(toLight);
            attenuation = 1.0f - saturate(dist / light.range);
            attenuation *= attenuation;
        }
        else if(light.type == LIGHT_SPOT)
        {
            float3 toLight = light.position.xyz - pin.worldPos;
            float dist = length(toLight);
            if (dist > light.range)
                continue;
            L = normalize(toLight);
            
            float cosAngle = dot(L, normalize(-light.direction.xyz));
            float cosInner = cos(light.spotInner);
            float cosOuter = cos(light.spotOuter);
            attenuation = saturate((cosAngle - cosOuter) / cosInner - cosOuter);
            attenuation *= 1.0f - saturate(dist / light.range);
        }
        
        float3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0f);
        if (NdotL <= 0.0f)
            continue;
        
        // Cook-Torrance BRDF
        float D = distributionGGX(N, H, rough);
        float G = geometrySmith(N, V, L, rough);
        float F = fresnelSchlick(max(dot(H, V), 0.0f), F0);
        
        float3 numerator = D * G * F;
        float denominator = 4.0f * max(dot(N, V), 0.0f) * NdotL + 0.0001f;
        float3 specular = numerator / denominator;
        
        float3 kS = F;
        float3 kD = (1.0f - kS) * (1.0f - metal);
        
        float3 radiance = lightColorRaw * intensity * attenuation;
        
        float shadow = (light.type == LIGHT_DIRECTIONAL)
                ? calcShadow(pin.worldPos, shadowBias) : 1.0f;
        
        Lo += (kD * albedo.rgb / PI + specular) * radiance * NdotL * shadow;
    }

    // ambient
    float3 ambient = ambientColor.rgb * albedo.rgb * ao;
    
    float3 color = ambient + Lo + emissive;
    
    // Tone Mapping
    color = color / (color + float3(1.0f, 1.0f, 1.0f));
    color = pow(color, float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));
    
    return float4(color, albedo.a);
}