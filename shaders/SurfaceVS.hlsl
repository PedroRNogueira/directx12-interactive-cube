struct VertexInput { float3 position : POSITION; };
struct VertexOutput { float3 position : POSITION; };

VertexOutput main(VertexInput input)
{
    // Control points continuam em espaço local; a superfície nasce no Domain Shader.
    VertexOutput output;
    output.position = input.position;
    return output;
}

