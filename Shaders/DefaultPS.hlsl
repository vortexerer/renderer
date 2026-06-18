#include "Common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

float4 main(VertexOutput input) : SV_Target
{
    float3 lightDir = normalize(float3(0.577f, 0.577f, 0.577f));
    float3 normal = normalize(input.Normal);
    float diffuse = max(dot(normal, lightDir), 0.0f);
    
    float4 texColor = g_Texture.Sample(g_Sampler, input.TexCoord);
    float3 ambient = float3(0.1f, 0.1f, 0.1f) * texColor.rgb;
    float3 litColor = texColor.rgb * diffuse + ambient;
    
    return float4(litColor, texColor.a);
}
