#include "Common.hlsl"

VertexOutput main(VertexInput input, uint viewID : SV_ViewID)
{
    VertexOutput output;
    
    float4 worldPos = mul(Model, float4(input.Position, 1.0f));
    float4 viewPos = mul(View[viewID], worldPos);
    output.Position = mul(Projection[viewID], viewPos);
    
    output.Normal = mul((float3x3)Model, input.Normal);
    output.TexCoord = input.TexCoord;
    output.ViewID = viewID;
    
    return output;
}
