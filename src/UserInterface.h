#pragma once

#include "SceneState.h"

#include <Windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

struct InterfaceStats
{
    float fps = 0.0f;
    std::string adapterName;
    UINT frameIndex = 0;
    UINT64 fenceValue = 0;
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float zoom = 0.0f;
};

class UserInterface
{
public:
    UserInterface() = default;
    ~UserInterface();
    UserInterface(const UserInterface&) = delete;
    UserInterface& operator=(const UserInterface&) = delete;

    void Initialize(HWND window, ID3D12Device* device, UINT frameCount);
    void BeginFrame();
    void Draw(RenderOptions& options, const InterfaceStats& stats, bool visible);
    void Render(ID3D12GraphicsCommandList* commandList);
    bool HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) const;
    [[nodiscard]] bool WantsMouse() const;
    [[nodiscard]] bool WantsKeyboard() const;
    [[nodiscard]] bool Initialized() const { return initialized_; }

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
    bool initialized_ = false;
};

