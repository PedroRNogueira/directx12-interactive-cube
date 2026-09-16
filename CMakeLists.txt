cmake_minimum_required(VERSION 3.24)

project(DirectX12InteractiveCube VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(DirectX12InteractiveCube WIN32
    src/main.cpp
    src/Application.cpp
    src/Application.h
    src/Camera.cpp
    src/Camera.h
    src/DxHelpers.h
    src/Mesh.cpp
    src/Mesh.h
    src/Renderer.cpp
    src/Renderer.h
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

