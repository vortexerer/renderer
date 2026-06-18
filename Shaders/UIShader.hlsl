#include "Common.hlsl"

Texture2D g_SDFAtlas : register(t0);
SamplerState g_Sampler : register(s0);

cbuffer cbUIColor : register(b1)
{
    float4 textColor;
};

float4 main(VertexOutput input) : SV_Target
{
    float dist = g_SDFAtlas.Sample(g_Sampler, input.TexCoord).r;
    float width = 0.05f;
    float alpha = smoothstep(0.5f - width, 0.5f + width, dist);
    
    if (alpha < 0.1f)
        discard;
        
    return float4(textColor.rgb, textColor.a * alpha);
}
