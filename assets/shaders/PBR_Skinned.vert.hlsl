cbuffer TransformCB : register(b0)
{
    matrix mvp;
    matrix world;
    matrix normalMatrix;
};

#define MAX_BONES 128
cbuffer SkinningCB : register(b4)
{
    matrix boneMatrices[MAX_BONES];
};


struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    int4 boneIndices : BONEINDICES;
    float4 boneWeights : BONEWEIGHTS;
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 normal   : TEXCOORD2;
    float3 tangent  : TEXCOORD3;
    float3 bitangent : TEXCOORD4;
};


PSInput VS(VSInput vin)
{
    
    PSInput vout;
    
    float4x4 skinMatrix = float4x4(
                            0,0,0,0,
                            0,0,0,0,
                            0,0,0,0,
                            0,0,0,0
                        );
    float totalWeight = 0.0f;
    
    [unroll]
    for (int i = 0; i < 4;++i)
    {
        if(vin.boneIndices[i]>= 0 && vin.boneWeights[i] > 0.0f)
        {
            skinMatrix += 
                boneMatrices[vin.boneIndices[i]] *
                vin.boneWeights[i];
            totalWeight +=
                vin.boneWeights[i];
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
    float4 skinnedNormal = mul(float4(vin.normal, 0.0f), skinMatrix);
    float4 skinnedTangent = mul(float4(vin.tangent, 0.0f), skinMatrix);
    float4 skinnedBiTangent = mul(float4(vin.bitangent, 0.0f), skinMatrix);
    
    vout.position = mul(skinnedPos, mvp);
    vout.uv = vin.uv;
    vout.worldPos   = mul(skinnedPos, world).xyz;
    vout.normal     = normalize(mul(skinnedNormal, normalMatrix).xyz);
    vout.tangent    = normalize(mul(skinnedTangent, normalMatrix).xyz);
    vout.bitangent  = normalize(mul(skinnedBiTangent, normalMatrix).xyz);
    
    return vout;
}