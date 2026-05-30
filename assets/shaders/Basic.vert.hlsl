cbuffer TransformCB : register(b0)
{
    matrix mvp;
    matrix world;
    matrix normalMatrix;
};


struct VSInput
{
    float3 position     : POSITION;
    float4 color        : COLOR;
    float2 uv           : TEXCOORD0;
    float3 normal       : NORMAL;
    float3 tangent      : TANGENT;
    float3 bitangent    : BINORMAL;
};

struct PSInput
{
    float4 position     : SV_Position;
    float4 color        : COLOR;
    float2 uv           : TEXCOORD0;
    float3 worldPos     : TEXCOORD1;
    float3 normal       : TEXCOORD2;
    float3 tangent      : TEXCOORD3;
    float3 bitangent    : TEXCOORD4;
};

PSInput VS(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), mvp);
    output.color = input.color;
    output.uv = input.uv;
    output.worldPos = mul(float4(input.position, 1.0f), world).xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0f), normalMatrix).xyz);
    output.tangent = normalize(mul(float4(input.tangent, 0.0f), normalMatrix).xyz);
    output.bitangent = normalize(mul(float4(input.bitangent, 0.0f), normalMatrix).xyz);
    return output;
}