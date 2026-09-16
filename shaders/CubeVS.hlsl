#include "SceneConstants.hlsli"

struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : NORMAL;
    float3 color : COLOR;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    // A GPU recebe números, não uma entidade abstrata "cubo".
    output.position = mul(modelViewProjection, float4(input.position, 1.0f));
    output.worldPosition = mul(model, float4(input.position, 1.0f)).xyz;
    output.worldNormal = normalize(mul((float3x3)model, input.normal));
    // Modo 3 transforma a posição local em cor para expor a interpolação.
    output.color = colorVisualization > 0.5f ? input.position * 0.5f + 0.5f : input.color;
    return output;
}
