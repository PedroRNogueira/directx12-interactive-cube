#include "Renderer.h"

#include "DxHelpers.h"
#include "Mesh.h"

#include <d3dcompiler.h>
#include <filesystem>
#include <cstring>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace
{
D3D12_HEAP_PROPERTIES UploadHeap()
{
    D3D12_HEAP_PROPERTIES properties{};
    properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    properties.CreationNodeMask = 1;
    properties.VisibleNodeMask = 1;
    return properties;
}

D3D12_HEAP_PROPERTIES DefaultHeap()
{
    D3D12_HEAP_PROPERTIES properties{};
    properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    properties.CreationNodeMask = 1;
    properties.VisibleNodeMask = 1;
    return properties;
}

D3D12_RESOURCE_DESC BufferDescription(UINT64 size)
{
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = size;
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return description;
}
}

Renderer::~Renderer()
{
    if (commandQueue_ && fence_ && fenceEvent_)
    {
        try { WaitForGpu(); } catch (...) {}
    }
    for (auto& frame : frames_)
    {
        if (frame.constantBuffer && frame.mappedConstants)
            frame.constantBuffer->Unmap(0, nullptr);
    }
    if (fenceEvent_) CloseHandle(fenceEvent_);
}

void Renderer::Initialize(HWND window, UINT width, UINT height)
{
    window_ = window;
    width_ = width;
    height_ = height;
    EnableDebugLayer();

    UINT factoryFlags = debugLayerEnabled_ ? DXGI_CREATE_FACTORY_DEBUG : 0;
    ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory_)),
                  "CreateDXGIFactory2");
    CreateDeviceAndQueue();
    CreateDescriptorHeaps();
    CreateSwapChain();
    CreateFrameResources();
    CreateRenderTargets();
    CreateDepthBuffer();
    CreateRootSignature();
    CreatePipelineStates();
    CreateGeometry();
    UpdateViewport(width, height);

    ThrowIfFailed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                        IID_PPV_ARGS(&fence_)), "CreateFence");
    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "CreateEvent");
}

void Renderer::EnableDebugLayer()
{
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    {
        debug->EnableDebugLayer();
        debugLayerEnabled_ = true;
    }
#endif
}

void Renderer::CreateDeviceAndQueue()
{
    ComPtr<IDXGIAdapter1> selected;
    for (UINT index = 0;
         factory_->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                              IID_PPV_ARGS(&selected)) != DXGI_ERROR_NOT_FOUND;
         ++index)
    {
        DXGI_ADAPTER_DESC1 description{};
        ThrowIfFailed(selected->GetDesc1(&description), "IDXGIAdapter::GetDesc1");
        if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
            SUCCEEDED(D3D12CreateDevice(selected.Get(), D3D_FEATURE_LEVEL_11_0,
                                        __uuidof(ID3D12Device), nullptr)))
        {
            adapterName_ = description.Description;
            break;
        }
        selected.Reset();
    }

    if (!selected)
    {
        ThrowIfFailed(factory_->EnumWarpAdapter(IID_PPV_ARGS(&selected)), "EnumWarpAdapter");
        DXGI_ADAPTER_DESC1 description{};
        ThrowIfFailed(selected->GetDesc1(&description), "WARP GetDesc1");
        adapterName_ = description.Description;
        usingWarp_ = true;
    }

    ThrowIfFailed(D3D12CreateDevice(selected.Get(), D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&device_)), "D3D12CreateDevice");

    D3D12_COMMAND_QUEUE_DESC description{};
    description.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(device_->CreateCommandQueue(&description, IID_PPV_ARGS(&commandQueue_)),
                  "CreateCommandQueue");
}

void Renderer::CreateDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtv{};
    rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv.NumDescriptors = FrameCount;
    ThrowIfFailed(device_->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&rtvHeap_)),
                  "CreateDescriptorHeap(RTV)");
    rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC dsv{};
    dsv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsv.NumDescriptors = 1;
    ThrowIfFailed(device_->CreateDescriptorHeap(&dsv, IID_PPV_ARGS(&dsvHeap_)),
                  "CreateDescriptorHeap(DSV)");
}

void Renderer::CreateSwapChain()
{
    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Width = width_;
    description.Height = height_;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = FrameCount;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    description.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory_->CreateSwapChainForHwnd(commandQueue_.Get(), window_, &description,
                                                    nullptr, nullptr, &swapChain),
                  "CreateSwapChainForHwnd");
    ThrowIfFailed(factory_->MakeWindowAssociation(window_, DXGI_MWA_NO_ALT_ENTER),
                  "MakeWindowAssociation");
    ThrowIfFailed(swapChain.As(&swapChain_), "Query IDXGISwapChain3");
}

void Renderer::CreateFrameResources()
{
    const UINT64 constantSize = AlignConstantBufferSize(sizeof(SceneConstants));
    const auto heap = UploadHeap();
    const auto buffer = BufferDescription(constantSize);
    for (auto& frame : frames_)
    {
        ThrowIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                       IID_PPV_ARGS(&frame.allocator)),
                      "CreateCommandAllocator");
        ThrowIfFailed(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
                         D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                         IID_PPV_ARGS(&frame.constantBuffer)), "Create constant buffer");
        D3D12_RANGE noRead{0, 0};
        ThrowIfFailed(frame.constantBuffer->Map(0, &noRead,
                         reinterpret_cast<void**>(&frame.mappedConstants)), "Map constant buffer");
    }

    ThrowIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                    frames_[0].allocator.Get(), nullptr, IID_PPV_ARGS(&commandList_)),
                  "CreateCommandList");
    ThrowIfFailed(commandList_->Close(), "Close initial command list");
}

void Renderer::CreateRenderTargets()
{
    auto handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    for (UINT index = 0; index < FrameCount; ++index)
    {
        ThrowIfFailed(swapChain_->GetBuffer(index, IID_PPV_ARGS(&renderTargets_[index])),
                      "SwapChain GetBuffer");
        device_->CreateRenderTargetView(renderTargets_[index].Get(), nullptr, handle);
        handle.ptr += rtvDescriptorSize_;
    }
}

void Renderer::CreateDepthBuffer()
{
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = width_;
    description.Height = height_;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_D32_FLOAT;
    description.SampleDesc.Count = 1;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clear{};
    clear.Format = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth = 1.0f;
    const auto heap = DefaultHeap();
    ThrowIfFailed(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
                    D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&depthBuffer_)),
                  "Create depth buffer");
    device_->CreateDepthStencilView(depthBuffer_.Get(), nullptr,
                                    dsvHeap_->GetCPUDescriptorHandleForHeapStart());
}

void Renderer::CreateRootSignature()
{
    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameter.Descriptor.ShaderRegister = 0;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC description{};
    description.NumParameters = 1;
    description.pParameters = &parameter;
    description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    HRESULT result = D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1,
                                                  &serialized, &errors);
    if (FAILED(result) && errors) OutputDebugStringA(static_cast<char*>(errors->GetBufferPointer()));
    ThrowIfFailed(result, "D3D12SerializeRootSignature");
    ThrowIfFailed(device_->CreateRootSignature(0, serialized->GetBufferPointer(),
                      serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature_)),
                  "CreateRootSignature");
}

ComPtr<ID3DBlob> Renderer::CompileShader(const wchar_t* file, const char* target) const
{
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    const auto path = std::filesystem::path(modulePath).parent_path() / L"shaders" / file;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    HRESULT result = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                        "main", target, flags, 0, &shader, &errors);
    if (FAILED(result) && errors) OutputDebugStringA(static_cast<char*>(errors->GetBufferPointer()));
    ThrowIfFailed(result, "D3DCompileFromFile");
    return shader;
}

void Renderer::CreatePipelineStates()
{
    const auto vertexShader = CompileShader(L"CubeVS.hlsl", "vs_5_1");
    const auto pixelShader = CompileShader(L"CubePS.hlsl", "ps_5_1");
    const D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = rootSignature_.Get();
    pso.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
    pso.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    pso.InputLayout = {layout, static_cast<UINT>(std::size(layout))};
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = UINT_MAX;

    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pso.RasterizerState.FrontCounterClockwise = FALSE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

    ThrowIfFailed(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&solidPipeline_)),
                  "Create solid PSO");
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    ThrowIfFailed(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&wireframePipeline_)),
                  "Create wireframe PSO");
}

void Renderer::CreateGeometry()
{
    const UINT vertexBytes = static_cast<UINT>(sizeof(CubeMesh::Vertices));
    const UINT indexBytes = static_cast<UINT>(sizeof(CubeMesh::Indices));
    const auto heap = UploadHeap();
    auto description = BufferDescription(vertexBytes);
    ThrowIfFailed(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer_)),
                  "Create vertex buffer");
    description = BufferDescription(indexBytes);
    ThrowIfFailed(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBuffer_)),
                  "Create index buffer");

    void* mapped = nullptr;
    D3D12_RANGE noRead{0, 0};
    ThrowIfFailed(vertexBuffer_->Map(0, &noRead, &mapped), "Map vertex buffer");
    std::memcpy(mapped, CubeMesh::Vertices.data(), vertexBytes);
    vertexBuffer_->Unmap(0, nullptr);
    ThrowIfFailed(indexBuffer_->Map(0, &noRead, &mapped), "Map index buffer");
    std::memcpy(mapped, CubeMesh::Indices.data(), indexBytes);
    indexBuffer_->Unmap(0, nullptr);

    vertexView_ = {vertexBuffer_->GetGPUVirtualAddress(), vertexBytes, sizeof(Vertex)};
    indexView_ = {indexBuffer_->GetGPUVirtualAddress(), indexBytes, DXGI_FORMAT_R16_UINT};
}

void Renderer::UpdateScene(const DirectX::XMMATRIX& mvp, float colorMode, float elapsedSeconds)
{
    // DirectXMath produz a matriz para vetores-linha. Em memória HLSL column-major,
    // os mesmos bytes representam a transposta, adequada a mul(matriz, vetor-coluna).
    DirectX::XMStoreFloat4x4(&pendingConstants_.modelViewProjection,
                            mvp);
    pendingConstants_.colorMode = colorMode;
    pendingConstants_.timeSeconds = elapsedSeconds;
}

void Renderer::Render(bool wireframe)
{
    const UINT index = swapChain_->GetCurrentBackBufferIndex();
    auto& frame = frames_[index];
    WaitForFrame(frame);
    std::memcpy(frame.mappedConstants, &pendingConstants_, sizeof(pendingConstants_));

    ThrowIfFailed(frame.allocator->Reset(), "CommandAllocator Reset");
    ThrowIfFailed(commandList_->Reset(frame.allocator.Get(),
                    wireframe ? wireframePipeline_.Get() : solidPipeline_.Get()),
                  "CommandList Reset");

    // O back buffer precisa estar no estado correto para receber escrita da GPU.
    auto toRender = TransitionBarrier(renderTargets_[index].Get(),
                                      D3D12_RESOURCE_STATE_PRESENT,
                                      D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRender);

    auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(index) * rtvDescriptorSize_;
    const auto dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    const float clearColor[] = {0.025f, 0.035f, 0.065f, 1.0f};
    commandList_->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    commandList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissor_);
    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    commandList_->SetGraphicsRootConstantBufferView(0,
                                                     frame.constantBuffer->GetGPUVirtualAddress());
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &vertexView_);
    commandList_->IASetIndexBuffer(&indexView_);
    commandList_->DrawIndexedInstanced(static_cast<UINT>(CubeMesh::Indices.size()), 1, 0, 0, 0);

    // Present só pode consumir um recurso novamente no estado PRESENT.
    auto toPresent = TransitionBarrier(renderTargets_[index].Get(),
                                       D3D12_RESOURCE_STATE_RENDER_TARGET,
                                       D3D12_RESOURCE_STATE_PRESENT);
    commandList_->ResourceBarrier(1, &toPresent);
    ThrowIfFailed(commandList_->Close(), "CommandList Close");
    ID3D12CommandList* lists[] = {commandList_.Get()};
    commandQueue_->ExecuteCommandLists(1, lists);

    ThrowIfFailed(swapChain_->Present(1, 0), "SwapChain Present (VSync)");
    const UINT64 signal = nextFenceValue_++;
    ThrowIfFailed(commandQueue_->Signal(fence_.Get(), signal), "CommandQueue Signal");
    frame.fenceValue = signal;
    lastSubmittedFence_ = signal;
}

void Renderer::WaitForFrame(FrameContext& frame)
{
    if (frame.fenceValue != 0 && fence_->GetCompletedValue() < frame.fenceValue)
    {
        ThrowIfFailed(fence_->SetEventOnCompletion(frame.fenceValue, fenceEvent_),
                      "Fence SetEventOnCompletion");
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void Renderer::WaitForGpu()
{
    const UINT64 signal = nextFenceValue_++;
    ThrowIfFailed(commandQueue_->Signal(fence_.Get(), signal), "Signal before GPU wait");
    if (fence_->GetCompletedValue() < signal)
    {
        ThrowIfFailed(fence_->SetEventOnCompletion(signal, fenceEvent_),
                      "Fence SetEventOnCompletion");
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void Renderer::Resize(UINT width, UINT height)
{
    if (!swapChain_ || width == 0 || height == 0 || (width == width_ && height == height_)) return;
    WaitForGpu();
    for (auto& target : renderTargets_) target.Reset();
    depthBuffer_.Reset();
    for (auto& frame : frames_) frame.fenceValue = 0;
    ThrowIfFailed(swapChain_->ResizeBuffers(FrameCount, width, height,
                                             DXGI_FORMAT_R8G8B8A8_UNORM, 0),
                  "SwapChain ResizeBuffers");
    width_ = width;
    height_ = height;
    CreateRenderTargets();
    CreateDepthBuffer();
    UpdateViewport(width, height);
}

void Renderer::UpdateViewport(UINT width, UINT height)
{
    viewport_ = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    scissor_ = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
}

UINT Renderer::FrameIndex() const
{
    return swapChain_ ? swapChain_->GetCurrentBackBufferIndex() : 0;
}

UINT64 Renderer::DebugMessageCount() const
{
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (device_ && SUCCEEDED(device_.As(&infoQueue)))
        return infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
#endif
    return 0;
}
