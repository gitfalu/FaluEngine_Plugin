cbuffer UITransformCB : register(b0)
{
    matrix orthoProjection;
    float2 position;
    float2 size;
    float rotation;
    float3 _pad;
};

struct VSInput
{
    float2 localPos : POSITION;
    float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

PSInput VS(VSInput vin)
{
    PSInput vout;
    
    float cosR = cos(rotation);
    float sinR = sin(rotation);
    float2 rotatedPos = float2(
        vin.localPos.x * cosR - vin.localPos.y * sinR,
        vin.localPos.x * sinR - vin.localPos.y * cosR
    );
    
    float2 worldPos = position + rotatedPos * size;
    
    vout.position = mul(float4(worldPos, 0.0f, 1.0f), orthoProjection);
    vout.uv = vin.uv;
    return vout;
}