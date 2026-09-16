#pragma once

#include <Windows.h>
#include <chrono>
#include <exception>
#include <memory>

#include "Camera.h"
#include "Renderer.h"

class Application
{
public:
    explicit Application(HINSTANCE instance);
    int Run(int showCommand);

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                             WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void CreateMainWindow(int showCommand);
    void Update(float deltaSeconds);
    void Render();
    void UpdateWindowTitle(float deltaSeconds);

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    UINT clientWidth_ = 1280;
    UINT clientHeight_ = 720;
    bool minimized_ = false;
    bool dragging_ = false;
    bool rendererReady_ = false;
    bool showTelemetry_ = true;
    POINT lastMouse_{};
    int renderMode_ = 1;
    float elapsedSeconds_ = 0.0f;
    float titleAccumulator_ = 0.0f;
    unsigned titleFrames_ = 0;
    float displayedFps_ = 0.0f;
    std::exception_ptr deferredError_;
    Camera camera_;
    std::unique_ptr<Renderer> renderer_;
};
