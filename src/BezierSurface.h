#pragma once

#include <DirectXMath.h>
#include <array>

struct BezierControlPoint
{
    DirectX::XMFLOAT3 position;
};

namespace BezierSurface
{
    // Um único patch bicúbico: 4 linhas x 4 colunas, sem malha densa na CPU.
    extern const std::array<BezierControlPoint, 16> ControlPoints;
}

