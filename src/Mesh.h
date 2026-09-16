#pragma once

#include <DirectXMath.h>
#include <array>
#include <cstdint>

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 color;
    DirectX::XMFLOAT3 normal;
};

namespace CubeMesh
{
    // Há 8 cantos conceituais, mas 24 vértices permitem uma cor por face.
    extern const std::array<Vertex, 24> Vertices;
    extern const std::array<std::uint16_t, 36> Indices;
}
