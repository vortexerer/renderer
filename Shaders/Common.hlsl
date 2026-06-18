cbuffer cbPerObject : register(b0)
{
    float4x4 View[2];
    float4x4 Projection[2];
    float4x4 Model;
};

struct VertexInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
    uint ViewID : SV_RenderTargetArrayIndex;
};
