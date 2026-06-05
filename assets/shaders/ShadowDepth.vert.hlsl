cbuffer ShadowCB : register(b0)
{
    matrix lightMVP;
};

float4 VS(float3 position : POSITION) : SV_Position
{
    return mul(float4(position, 1.0f), lightMVP);
}