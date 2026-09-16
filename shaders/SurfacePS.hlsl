#include "Lighting.hlsli"

struct PixelInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : NORMAL;
    float3 color : COLOR;
};

float4 main(PixelInput input) : SV_TARGET
{
    return float4(ApplyLighting(input.color, input.worldPosition, input.worldNormal), 1.0f);
}

