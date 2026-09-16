#include "SceneConstants.hlsli"

struct ControlPoint { float3 position : POSITION; };
struct PatchFactors
{
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};
struct DomainOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : NORMAL;
    float3 color : COLOR;
};

void Bernstein(float t, out float basis[4], out float derivative[4])
{
    const float s = 1.0f - t;
    basis[0] = s * s * s;
    basis[1] = 3.0f * t * s * s;
    basis[2] = 3.0f * t * t * s;
    basis[3] = t * t * t;
    derivative[0] = -3.0f * s * s;
    derivative[1] = 3.0f * s * s - 6.0f * t * s;
    derivative[2] = 6.0f * t * s - 3.0f * t * t;
    derivative[3] = 3.0f * t * t;
}

[domain("quad")]
DomainOutput main(PatchFactors factors, float2 uv : SV_DomainLocation,
                  const OutputPatch<ControlPoint, 16> patch)
{
    float bu[4], dbu[4], bv[4], dbv[4];
    Bernstein(uv.x, bu, dbu);
    Bernstein(uv.y, bv, dbv);

    float3 localPosition = 0.0f;
    float3 derivativeU = 0.0f;
    float3 derivativeV = 0.0f;
    [unroll] for (int row = 0; row < 4; ++row)
    {
        [unroll] for (int column = 0; column < 4; ++column)
        {
            const float3 controlPosition = patch[row * 4 + column].position;
            localPosition += controlPosition * bu[column] * bv[row];
            derivativeU += controlPosition * dbu[column] * bv[row];
            derivativeV += controlPosition * bu[column] * dbv[row];
        }
    }

    // A ordem dP/dv x dP/du aponta para +Y nesta grade XZ.
    const float3 localNormal = normalize(cross(derivativeV, derivativeU));
    const float4 world = mul(model, float4(localPosition, 1.0f));

    DomainOutput output;
    output.position = mul(modelViewProjection, float4(localPosition, 1.0f));
    output.worldPosition = world.xyz;
    output.worldNormal = normalize(mul((float3x3)model, localNormal));
    output.color = colorVisualization > 0.5f
                 ? float3(uv.x, uv.y, 1.0f - uv.x)
                 : lerp(float3(0.08f, 0.32f, 0.78f), float3(0.15f, 0.82f, 0.70f), uv.y);
    return output;
}
