#include "UserInterface.h"

#include "DxHelpers.h"

#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#include <cmath>
#include <stdexcept>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message,
                                                             WPARAM wParam, LPARAM lParam);

UserInterface::~UserInterface()
{
    if (!initialized_) return;
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void UserInterface::Initialize(HWND window, ID3D12Device* device, UINT frameCount)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDescription.NumDescriptors = 1;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&heapDescription,
                                                IID_PPV_ARGS(&descriptorHeap_)),
                  "CreateDescriptorHeap(ImGui)");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.ItemSpacing = ImVec2(8.0f, 6.0f);

    if (!ImGui_ImplWin32_Init(window))
        throw std::runtime_error("ImGui_ImplWin32_Init falhou");
    if (!ImGui_ImplDX12_Init(device, static_cast<int>(frameCount),
                             DXGI_FORMAT_R8G8B8A8_UNORM, descriptorHeap_.Get(),
                             descriptorHeap_->GetCPUDescriptorHandleForHeapStart(),
                             descriptorHeap_->GetGPUDescriptorHandleForHeapStart()))
        throw std::runtime_error("ImGui_ImplDX12_Init falhou");
    initialized_ = true;
}

void UserInterface::BeginFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void UserInterface::Draw(RenderOptions& options, const InterfaceStats& stats, bool visible)
{
    if (visible)
    {
        ImGui::SetNextWindowPos(ImVec2(14.0f, 14.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(390.0f, 690.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(345.0f, 420.0f), ImVec2(460.0f, 1000.0f));
        ImGui::Begin("DirectX 12 - Painel da Demonstracao", nullptr, ImGuiWindowFlags_NoCollapse);

        ImGui::SeparatorText("OBJETO");
        const int selectedObject = options.object == SceneObject::Cube ? 0 : 1;
        if (ImGui::RadioButton("Cubo", selectedObject == 0)) options.object = SceneObject::Cube;
        ImGui::SameLine();
        if (ImGui::RadioButton("Superficie Bezier", selectedObject == 1))
            options.object = SceneObject::BezierSurface;

        ImGui::SeparatorText("VISUALIZACAO");
        ImGui::Checkbox("Wireframe", &options.wireframe);
        ImGui::SameLine();
        ImGui::Checkbox("Iluminacao", &options.lighting);
        ImGui::Checkbox("Componente especular", &options.specular);
        ImGui::SameLine();
        ImGui::Checkbox("Auto-rotacao", &options.autoRotate);
        ImGui::Checkbox("Cor pela posicao", &options.colorVisualization);

        ImGui::SeparatorText("TESSELLATION DA GPU");
        ImGui::SliderFloat("Fator", &options.tessellationFactor, 1.0f, 32.0f, "%.0f");
        options.tessellationFactor = std::round(options.tessellationFactor);
        if (options.object == SceneObject::Cube)
            ImGui::TextDisabled("Usado pela superficie Bezier (16 control points).");

        ImGui::SeparatorText("LUZ E MATERIAL");
        ImGui::SliderFloat("Intensidade", &options.lightIntensity, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat3("Direcao", &options.lightDirection.x, -1.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Ambiente", &options.ambientIntensity, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Especular", &options.specularIntensity, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Shininess", &options.shininess, 2.0f, 128.0f, "%.0f");

        ImGui::SeparatorText("TELEMETRIA REAL");
        ImGui::Text("FPS: %.1f", stats.fps);
        ImGui::TextWrapped("GPU: %s", stats.adapterName.c_str());
        ImGui::Text("Back buffer: %u / 3", stats.frameIndex);
        ImGui::Text("Fence Value: %llu", static_cast<unsigned long long>(stats.fenceValue));
        ImGui::Text("Yaw %.1f | Pitch %.1f | Zoom %.1f",
                    stats.yawDegrees, stats.pitchDegrees, stats.zoom);
        ImGui::Text("Objeto: %s", options.object == SceneObject::Cube ? "Cubo" : "Bezier");
        ImGui::Text("Iluminacao %s | Wireframe %s",
                    options.lighting ? "ON" : "OFF", options.wireframe ? "ON" : "OFF");

        if (ImGui::CollapsingHeader("Pipeline", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextUnformatted("CPU -> Command List -> Command Queue -> GPU");
            ImGui::TextUnformatted("Input Assembler -> Vertex Shader");
            if (options.object == SceneObject::BezierSurface)
                ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f),
                                   "Hull Shader -> Tessellator -> Domain Shader");
            ImGui::TextUnformatted("Rasterizer -> Pixel Shader -> Output Merger");
            ImGui::TextUnformatted("Back Buffer -> Present -> Windows -> Monitor");
        }

        if (ImGui::CollapsingHeader("Controles"))
        {
            ImGui::TextUnformatted("Mouse esquerdo + arrastar: orbitar");
            ImGui::TextUnformatted("Scroll: zoom | R: reset | Espaco: auto-rotacao");
            ImGui::TextUnformatted("1: cubo | 2: superficie | W: wireframe");
            ImGui::TextUnformatted("L: iluminacao | F1: interface | Esc: sair");
        }
        ImGui::End();
    }
    ImGui::Render();
}

void UserInterface::Render(ID3D12GraphicsCommandList* commandList)
{
    ID3D12DescriptorHeap* heaps[] = {descriptorHeap_.Get()};
    commandList->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

bool UserInterface::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) const
{
    return initialized_ && ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam) != 0;
}

bool UserInterface::WantsMouse() const { return initialized_ && ImGui::GetIO().WantCaptureMouse; }
bool UserInterface::WantsKeyboard() const { return initialized_ && ImGui::GetIO().WantCaptureKeyboard; }

