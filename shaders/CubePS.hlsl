struct PixelInput { float4 position : SV_POSITION; float3 color : COLOR; };

float4 main(PixelInput input) : SV_TARGET
{
    // O rasterizador interpola a cor antes desta função.
    return float4(input.color, 1.0f);
}
