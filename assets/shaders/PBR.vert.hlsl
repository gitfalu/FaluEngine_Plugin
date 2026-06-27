cbuffer TransformCB : register(b0)
{
    matrix mvp;
    matrix world;
    matrix normalMatrix;
};

struct VSInput
{
    float3 positoin : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
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
    vout.position = mul(float4(vin.positoin, 1.0f), mvp);
    vout.uv = vin.uv;
    vout.worldPos   = mul(float4(vin.positoin, 1.0f), world).xyz;
    vout.normal     = normalize(mul(float4(vin.normal, 0.0f), normalMatrix).xyz);
    vout.tangent    = normalize(mul(float4(vin.tangent, 0.0f), normalMatrix).xyz);
    vout.bitangent  = normalize(mul(float4(vin.bitangent, 0.0f), normalMatrix).xyz);
    
    return vout;
}