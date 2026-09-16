cbuffer SceneConstants : register(b0)
{
    float4x4 modelViewProjection;
    float colorMode;
    float timeSeconds;
    float2 padding;
};

struct VertexInput { float3 position : POSITION; float3 color : COLOR; };
struct VertexOutput { float4 position : SV_POSITION; float3 color : COLOR; };

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    // A GPU recebe números, não uma entidade abstrata "cubo".
    output.position = mul(modelViewProjection, float4(input.position, 1.0f));
    // Modo 3 transforma a posição local em cor para expor a interpolação.
    output.color = colorMode > 0.5f ? input.position * 0.5f + 0.5f : input.color;
    return output;
}
