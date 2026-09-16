#include "Camera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

Camera::Camera() = default;

void Camera::Update(float deltaSeconds, bool beingControlled, bool autoRotate)
{
    if (autoRotate && !beingControlled)
        targetYaw_ += deltaSeconds * 0.35f;

    // Aproximação exponencial: suave e independente da taxa de quadros.
    const float blend = 1.0f - std::exp(-10.0f * deltaSeconds);
    yaw_ += (targetYaw_ - yaw_) * blend;
    pitch_ += (targetPitch_ - pitch_) * blend;
    distance_ += (targetDistance_ - distance_) * blend;
}

void Camera::Rotate(float deltaX, float deltaY)
{
    targetYaw_ += deltaX * 0.010f;
    targetPitch_ = std::clamp(targetPitch_ + deltaY * 0.010f, -1.35f, 1.35f);
}

void Camera::Zoom(float wheelSteps)
{
    targetDistance_ = std::clamp(targetDistance_ - wheelSteps * 0.55f, 3.0f, 12.0f);
}

void Camera::Reset()
{
    targetYaw_ = 0.65f;
    targetPitch_ = 0.35f;
    targetDistance_ = 6.0f;
}

XMFLOAT3 Camera::Position() const
{
    return {
        distance_ * std::cos(pitch_) * std::sin(yaw_),
        distance_ * std::sin(pitch_),
       -distance_ * std::cos(pitch_) * std::cos(yaw_)
    };
}

XMMATRIX Camera::ViewMatrix() const
{
    const XMFLOAT3 position = Position();
    return XMMatrixLookAtLH(XMLoadFloat3(&position), XMVectorZero(), XMVectorSet(0, 1, 0, 0));
}

float Camera::YawDegrees() const { return XMConvertToDegrees(yaw_); }
float Camera::PitchDegrees() const { return XMConvertToDegrees(pitch_); }
