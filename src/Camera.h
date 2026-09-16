#pragma once

#include <DirectXMath.h>

class Camera
{
public:
    Camera();

    void Update(float deltaSeconds, bool beingControlled, bool autoRotate);
    void Rotate(float deltaX, float deltaY);
    void Zoom(float wheelSteps);
    void Reset();

    [[nodiscard]] DirectX::XMMATRIX ViewMatrix() const;
    [[nodiscard]] DirectX::XMFLOAT3 Position() const;
    [[nodiscard]] float YawDegrees() const;
    [[nodiscard]] float PitchDegrees() const;
    [[nodiscard]] float Distance() const { return distance_; }

private:
    float yaw_ = 0.65f;
    float pitch_ = 0.35f;
    float distance_ = 6.0f;
    float targetYaw_ = yaw_;
    float targetPitch_ = pitch_;
    float targetDistance_ = distance_;
};
