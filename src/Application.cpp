#include "Application.h"

#include <DirectXMath.h>
#include <windowsx.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace DirectX;

namespace
{
constexpr wchar_t WindowClassName[] = L"DirectX12InteractiveCubeWindow";
constexpr wchar_t BaseTitle[] = L"DirectX 12 Interactive Cube";
}

Application::Application(HINSTANCE instance) : instance_(instance), renderer_(std::make_unique<Renderer>()) {}

void Application::CreateMainWindow(int showCommand)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = WindowClassName;
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        throw std::runtime_error("RegisterClassExW falhou");

    RECT rectangle{0, 0, static_cast<LONG>(clientWidth_), static_cast<LONG>(clientHeight_)};
    AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
    window_ = CreateWindowExW(0, WindowClassName, BaseTitle, WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              rectangle.right - rectangle.left,
                              rectangle.bottom - rectangle.top,
                              nullptr, nullptr, instance_, this);
    if (!window_) throw std::runtime_error("CreateWindowExW falhou");
    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
}

int Application::Run(int showCommand)
{
    CreateMainWindow(showCommand);
    renderer_->Initialize(window_, clientWidth_, clientHeight_);
    userInterface_ = std::make_unique<UserInterface>();
    userInterface_->Initialize(window_, renderer_->Device(), Renderer::FrameCount);
    rendererReady_ = true;

    MSG message{};
    auto previous = std::chrono::steady_clock::now();
    while (message.message != WM_QUIT)
    {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        else if (minimized_)
        {
            WaitMessage();
            previous = std::chrono::steady_clock::now();
        }
        else
        {
            const auto now = std::chrono::steady_clock::now();
            const float delta = std::min(std::chrono::duration<float>(now - previous).count(), 0.1f);
            previous = now;
            Update(delta);
            Render();
        }
    }

    if (deferredError_) std::rethrow_exception(deferredError_);
    renderer_->WaitForGpu();
    return static_cast<int>(message.wParam);
}

void Application::Update(float deltaSeconds)
{
    elapsedSeconds_ += deltaSeconds;
    camera_.Update(deltaSeconds, dragging_, renderOptions_.autoRotate);

    const XMMATRIX model = XMMatrixIdentity();
    const XMMATRIX view = camera_.ViewMatrix();
    const float aspect = static_cast<float>(clientWidth_) / static_cast<float>(clientHeight_);
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), aspect, 0.1f, 100.0f);

    // A ordem row-vector do DirectXMath é Model * View * Projection.
    const XMMATRIX mvp = model * view * projection;
    renderer_->UpdateScene(mvp, renderOptions_.colorVisualization ? 1.0f : 0.0f, elapsedSeconds_);
    UpdateWindowTitle(deltaSeconds);

    InterfaceStats stats;
    stats.fps = displayedFps_;
    const int required = WideCharToMultiByte(CP_UTF8, 0, renderer_->AdapterName().c_str(), -1,
                                              nullptr, 0, nullptr, nullptr);
    if (required > 1)
    {
        stats.adapterName.resize(static_cast<size_t>(required));
        WideCharToMultiByte(CP_UTF8, 0, renderer_->AdapterName().c_str(), -1,
                            stats.adapterName.data(), required, nullptr, nullptr);
        stats.adapterName.pop_back();
    }
    stats.frameIndex = renderer_->FrameIndex();
    stats.fenceValue = renderer_->LastFenceValue();
    stats.yawDegrees = camera_.YawDegrees();
    stats.pitchDegrees = camera_.PitchDegrees();
    stats.zoom = camera_.Distance();
    userInterface_->BeginFrame();
    userInterface_->Draw(renderOptions_, stats, showInterface_);
}

void Application::Render()
{
    renderer_->Render(renderOptions_.wireframe, userInterface_.get());
}

void Application::UpdateWindowTitle(float deltaSeconds)
{
    titleAccumulator_ += deltaSeconds;
    ++titleFrames_;
    if (titleAccumulator_ < 0.5f) return;
    displayedFps_ = static_cast<float>(titleFrames_) / titleAccumulator_;
    titleAccumulator_ = 0.0f;
    titleFrames_ = 0;

    std::wostringstream title;
    title << BaseTitle << L" | FPS " << std::fixed << std::setprecision(1) << displayedFps_
          << L" | buffer " << renderer_->FrameIndex() << L"/" << Renderer::FrameCount
          << L" | fence " << renderer_->LastFenceValue()
          << L" | " << SceneObjectName(renderOptions_.object)
          << L" | wire " << (renderOptions_.wireframe ? L"on" : L"off")
          << L" | luz " << (renderOptions_.lighting ? L"on" : L"off")
          << L" | auto " << (renderOptions_.autoRotate ? L"on" : L"off")
          << L" | yaw " << camera_.YawDegrees() << L"° pitch " << camera_.PitchDegrees()
          << L"° zoom " << camera_.Distance()
          << L" | GPU: " << renderer_->AdapterName()
          << (renderer_->UsingWarp() ? L" (WARP/software)" : L"");
#if defined(_DEBUG)
    title << L" | D3D12 debug " << renderer_->DebugMessageCount();
#endif
    SetWindowTextW(window_, title.str().c_str());
}

LRESULT CALLBACK Application::WindowProcedure(HWND window, UINT message,
                                               WPARAM wParam, LPARAM lParam)
{
    Application* application = nullptr;
    if (message == WM_NCCREATE)
    {
        const auto create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        application = static_cast<Application*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
    }
    else
    {
        application = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }
    return application ? application->HandleMessage(window, message, wParam, lParam)
                       : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT Application::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    try
    {
        if (userInterface_ && userInterface_->Initialized())
            userInterface_->HandleMessage(window, message, wParam, lParam);

        switch (message)
        {
        case WM_SIZE:
            clientWidth_ = LOWORD(lParam);
            clientHeight_ = HIWORD(lParam);
            minimized_ = wParam == SIZE_MINIMIZED || clientWidth_ == 0 || clientHeight_ == 0;
            if (rendererReady_ && !minimized_) renderer_->Resize(clientWidth_, clientHeight_);
            return 0;
        case WM_LBUTTONDOWN:
            if (userInterface_ && userInterface_->WantsMouse()) return 0;
            dragging_ = true;
            lastMouse_ = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            SetCapture(window);
            return 0;
        case WM_MOUSEMOVE:
            if (userInterface_ && userInterface_->WantsMouse()) return 0;
            if (dragging_)
            {
                const POINT current{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                camera_.Rotate(static_cast<float>(current.x - lastMouse_.x),
                               static_cast<float>(current.y - lastMouse_.y));
                lastMouse_ = current;
            }
            return 0;
        case WM_LBUTTONUP:
            dragging_ = false;
            ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED:
            dragging_ = false;
            return 0;
        case WM_MOUSEWHEEL:
            if (userInterface_ && userInterface_->WantsMouse()) return 0;
            camera_.Zoom(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(window);
            else if (wParam == VK_F1 && (lParam & (1LL << 30)) == 0)
                showInterface_ = !showInterface_;
            else if (userInterface_ && userInterface_->WantsKeyboard()) return 0;
            else if (wParam == 'R') camera_.Reset();
            else if (wParam == VK_SPACE && (lParam & (1LL << 30)) == 0)
                renderOptions_.autoRotate = !renderOptions_.autoRotate;
            else if (wParam == 'W' && (lParam & (1LL << 30)) == 0)
                renderOptions_.wireframe = !renderOptions_.wireframe;
            else if (wParam == 'L' && (lParam & (1LL << 30)) == 0)
                renderOptions_.lighting = !renderOptions_.lighting;
            else if (wParam == 'C' && (lParam & (1LL << 30)) == 0)
                renderOptions_.colorVisualization = !renderOptions_.colorVisualization;
            else if (wParam == '1') renderOptions_.object = SceneObject::Cube;
            else if (wParam == '2') renderOptions_.object = SceneObject::BezierSurface;
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
        }
    }
    catch (...)
    {
        deferredError_ = std::current_exception();
        PostQuitMessage(EXIT_FAILURE);
        return 0;
    }
}
