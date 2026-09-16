#pragma once

#include <DirectXMath.h>

enum class SceneObject { Cube, BezierSurface };

struct RenderOptions
{
    SceneObject object = SceneObject::Cube;
    bool wireframe = false;
    bool lighting = true;
    bool specular = true;
    bool autoRotate = true;
    bool colorVisualization = false;
    float tessellationFactor = 8.0f;
    DirectX::XMFLOAT3 lightDirection{-0.45f, -1.0f, 0.35f};
    float lightIntensity = 1.15f;
    float ambientIntensity = 0.18f;
    float specularIntensity = 0.65f;
    float shininess = 48.0f;
};

inline const wchar_t* SceneObjectName(SceneObject object)
{
    return object == SceneObject::Cube ? L"Cubo" : L"Superficie Bezier";
}

