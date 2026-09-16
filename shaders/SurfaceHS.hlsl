#include "SceneConstants.hlsli"

struct ControlPoint { float3 position : POSITION; };
struct PatchFactors
{
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};

PatchFactors CalculatePatchFactors(InputPatch<ControlPoint, 16> patch, uint patchId : SV_PrimitiveID)
{
    PatchFactors factors;
    const float level = clamp(tessellationFactor, 1.0f, 32.0f);
    [unroll] for (int i = 0; i < 4; ++i) factors.edges[i] = level;
    factors.inside[0] = level;
    factors.inside[1] = level;
    return factors;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(16)]
[patchconstantfunc("CalculatePatchFactors")]
ControlPoint main(InputPatch<ControlPoint, 16> patch,
                  uint pointId : SV_OutputControlPointID,
                  uint patchId : SV_PrimitiveID)
{
    return patch[pointId];
}

