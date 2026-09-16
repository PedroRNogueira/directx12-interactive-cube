cmake_minimum_required(VERSION 3.24)

project(DirectX12InteractiveCube VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
target_compile_definitions(imgui PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(DirectX12InteractiveCube WIN32
    src/main.cpp
    src/Application.cpp
    src/Application.h
    src/BezierSurface.cpp
    src/BezierSurface.h
    src/Camera.cpp
    src/Camera.h
    src/DxHelpers.h
    src/Mesh.cpp
    src/Mesh.h
    src/Renderer.cpp
    src/Renderer.h
    src/SceneState.h
    src/UserInterface.cpp
    src/UserInterface.h
)

target_compile_definitions(DirectX12InteractiveCube PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(DirectX12InteractiveCube PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

target_link_libraries(DirectX12InteractiveCube PRIVATE
    imgui
    d3d12
    dxgi
    d3dcompiler
    dxguid
)

add_custom_command(TARGET DirectX12InteractiveCube POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:DirectX12InteractiveCube>/shaders"
    COMMENT "Copiando shaders HLSL para o diretorio do executavel"
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT DirectX12InteractiveCube)
