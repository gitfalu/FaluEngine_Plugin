TextureCube gEnvironmentMap : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 position : SV_Position;
    float3 localPos : TEXCOORD0;
};

static const float PI = 3.14159265359f;

float4 PS(PSInput pin) : SV_TARGET
{
    float3 N = normalize(pin.localPos);
    
    float3 up = abs(N.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));
    
    float3 irradiance = float3(0, 0, 0);
    float sampleDelta = 0.05f;
    float sampleCount = 0.0f;
    
    for (float phi = 0.0f; phi < 2.0f * PI;phi += sampleDelta)
    {
        for (float theta = 0.0f; theta < 0.5f * PI;theta += sampleDelta)
        {
            float3 tangentSample = float3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );
            
            float3 sampleVec =  tangentSample.x * right +
                                tangentSample.y * up +
                                tangentSample.z * N;
            
            irradiance += gEnvironmentMap.Sample(gSampler, sampleVec).rgb *
                            cos(theta) * sin(theta);
            sampleCount += 1.0f;
        }
    }
    
    irradiance = PI * irradiance * (1.0f / sampleCount);
    return float4(irradiance, 1.0f);

}