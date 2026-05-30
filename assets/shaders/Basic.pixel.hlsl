#define MAX_LIGHTS 16
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2

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
    float3 cameraPos;
    float4 ambientColor;
    float4 _pad;
};

cbuffer MaterialCB : register(b1)
{
    int useTexture;
    int useNormalMap;
    float shininess;
    float _mpad;
};

Texture2D gTexture : register(t0);
Texture2D gNormalMap : register(t1);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float3 tangent : TEXCOORD3;
    float3 bitangent : TEXCOORD4;
};

float3 calcPhong(float3 normal,float3 worldPos,float3 viewDir,
                 float3 lightDir,float3 lightColor,float intensity)
{
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = diff * lightColor * intensity;
    
    // Calc Specular(Phong)
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    float3 specular = spec * lightColor * intensity * 0.5f;
    
    return diffuse * specular;
}


float4 PS(PSInput input) : SV_TARGET
{
    // ベースカラー
    float4 baseColor = useTexture
        ? gTexture.Sample(gSampler, input.uv) * input.color
        : input.color;
    // 法線取得
    float3 normal = normalize(input.normal);
    if (useNormalMap)
    {
        float3 n = gNormalMap.Sample(gSampler, input.uv).xyz * 2.0f - 1.0f;
        float3 T = normalize(input.tangent);
        float3 B = normalize(input.bitangent);
        float3 N = normalize(input.normal);
        float3x3 TBN = float3x3(T, B, N);
        normal = normalize(mul(n, TBN));
    }
   
    float3 viewDir = normalize(cameraPos - input.worldPos);
    
    // ambient
    float3 result = ambientColor.rgb * baseColor.rgb;
    
    // 各ライトの寄与を加算
    for (int i = 0; i < lightCount; ++i)
    {
        LightData light = lights[i];
        float3 lightColor = light.color.rgb;
        float intensity = light.color.w;
        float3 lightDir = float3(0, 0, 0);
        float attenuation = 1.0f;
        
        if (light.type == LIGHT_DIRECTIONAL)
        {
            lightDir = normalize(-light.direction.xyz);
        }
        else if (light.type == LIGHT_POINT)
        {
            float3 toLight = light.position.xyz - input.worldPos;
            float dist = length(toLight);
            if (dist > light.range)
                continue;
            lightDir = normalize(toLight);
            attenuation = 1.0f - saturate(dist / light.range);
            attenuation *= attenuation;
        }
        else if(light.type == LIGHT_SPOT)
        {
            float3 toLight = light.position.xyz - input.worldPos;
            float dist = length(toLight);
            if (dist > light.range)
                continue;
            lightDir = normalize(toLight);
            
            float cosAngle = dot(lightDir, normalize(-light.direction.xyz));
            float cosInner = cos(light.spotInner);
            float cosOuter = cos(light.spotOuter);
            attenuation = saturate((cosAngle - cosOuter) / (cosInner - cosOuter));
            attenuation *= 1.0f - saturate(dist / light.range);
        }

        result += calcPhong(normal, input.worldPos, viewDir,
                            lightDir, lightColor, intensity) *
                  attenuation * baseColor.rgb;
    }
    
    return float4(saturate(result),baseColor.a);
}