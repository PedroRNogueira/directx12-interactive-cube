#pragma once

#include <Windows.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <string>

#include "SceneState.h"

class UserInterface;

class Renderer
{
public:
    static constexpr UINT FrameCount = 3;

    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void Initialize(HWND window, UINT width, UINT height);
    void UpdateScene(const DirectX::XMMATRIX& model,
                     const DirectX::XMMATRIX& view,
                     const DirectX::XMMATRIX& projection,
                     const DirectX::XMFLOAT3& cameraPosition,
                     const RenderOptions& options);
    void Render(const RenderOptions& options, UserInterface* userInterface);
    void Resize(UINT width, UINT height);
    void WaitForGpu();

    [[nodiscard]] UINT FrameIndex() const;
    [[nodiscard]] UINT64 LastFenceValue() const { return lastSubmittedFence_; }
    [[nodiscard]] const std::wstring& AdapterName() const { return adapterName_; }
    [[nodiscard]] bool UsingWarp() const { return usingWarp_; }
    [[nodiscard]] UINT64 DebugMessageCount() const;
    [[nodiscard]] ID3D12Device* Device() const { return device_.Get(); }

private:
    struct FrameContext
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
        Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;
        std::byte* mappedConstants = nullptr;
        UINT64 fenceValue = 0;
    };

    struct SceneConstants
    {
        DirectX::XMFLOAT4X4 modelViewProjection;
        DirectX::XMFLOAT4X4 model;
        DirectX::XMFLOAT3 cameraPosition;
        float lightingEnabled = 1.0f;
        DirectX::XMFLOAT3 lightDirection;
        float lightIntensity = 1.0f;
        float ambientIntensity = 0.18f;
        float specularIntensity = 0.65f;
        float shininess = 48.0f;
        float tessellationFactor = 8.0f;
        float specularEnabled = 1.0f;
        float colorVisualization = 0.0f;
        float padding[2]{};
    };
    static_assert(sizeof(SceneConstants) == 192,
                  "O layout C++ deve corresponder ao cbuffer HLSL de 192 bytes");

    void EnableDebugLayer();
    void CreateDeviceAndQueue();
    void CreateSwapChain();
    void CreateDescriptorHeaps();
    void CreateFrameResources();
    void CreateRenderTargets();
    void CreateDepthBuffer();
    void CreateRootSignature();
    void CreatePipelineStates();
    void CreateGeometry();
    void UpdateViewport(UINT width, UINT height);
    void WaitForFrame(FrameContext& frame);
    Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const wchar_t* file,
                                                    const char* target) const;

    HWND window_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
    bool debugLayerEnabled_ = false;
    bool usingWarp_ = false;
    std::wstring adapterName_;

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    std::array<FrameContext, FrameCount> frames_;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    UINT rtvDescriptorSize_ = 0;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount> renderTargets_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> cubeSolidPipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> cubeWireframePipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> surfaceSolidPipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> surfaceWireframePipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> surfaceControlPointBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexView_{};
    D3D12_INDEX_BUFFER_VIEW indexView_{};
    D3D12_VERTEX_BUFFER_VIEW surfaceControlPointView_{};

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;
    UINT64 nextFenceValue_ = 1;
    UINT64 lastSubmittedFence_ = 0;
    SceneConstants pendingConstants_{};
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissor_{};
};
