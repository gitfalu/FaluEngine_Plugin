cbuffer ShadowCB : register(b0)
{
    matrix lightMVP;
};

#define MAX_BONES 128

cbuffer SkinningCB : register(b4)
{
    matrix boneMatrices[MAX_BONES];
};

struct VSInput
{
    float3 position : POSITION;
    int4 boneIndices : BONEINDICES;
    float4 boneWeights : BONEWEIGHTS;
};

float4 VS(VSInput vin) : SV_Position
{
    float4x4 skinMatrix = float4x4(
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    );
    
    float totalWeight = 0.0f;
    
    [unroll]
    for (int i = 0; i < 4;++i)
    {
        if (vin.boneIndices[i] >= 0 && vin.boneWeights[i] > 0.0f)
        {
            skinMatrix += boneMatrices[vin.boneIndices[i]] * vin.boneWeights[i];
            totalWeight += vin.boneWeights[i];
        }

    }
    
    if(totalWeight < 0.0001f)
    {
        skinMatrix = float4x4(
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        );
    }
    
    float4 skinnedPos = mul(float4(vin.position, 1.0f), skinMatrix);
    return mul(skinnedPos,lightMVP);
}