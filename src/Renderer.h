#pragma once

#include <Windows.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <string>

class Renderer
{
public:
    static constexpr UINT FrameCount = 3;

    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void Initialize(HWND window, UINT width, UINT height);
    void UpdateScene(const DirectX::XMMATRIX& modelViewProjection,
                     float colorMode, float elapsedSeconds);
    void Render(bool wireframe);
    void Resize(UINT width, UINT height);
    void WaitForGpu();

    [[nodiscard]] UINT FrameIndex() const;
    [[nodiscard]] UINT64 LastFenceValue() const { return lastSubmittedFence_; }
    [[nodiscard]] const std::wstring& AdapterName() const { return adapterName_; }
    [[nodiscard]] bool UsingWarp() const { return usingWarp_; }
    [[nodiscard]] UINT64 DebugMessageCount() const;

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
        float colorMode = 0.0f;
        float timeSeconds = 0.0f;
        float padding[2]{};
    };

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
    Microsoft::WRL::ComPtr<ID3D12PipelineState> solidPipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> wireframePipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexView_{};
    D3D12_INDEX_BUFFER_VIEW indexView_{};

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;
    UINT64 nextFenceValue_ = 1;
    UINT64 lastSubmittedFence_ = 0;
    SceneConstants pendingConstants_{};
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissor_{};
};
