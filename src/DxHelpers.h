#pragma once

#include <Windows.h>
#include <d3d12.h>

#include <cstdio>
#include <stdexcept>
#include <string>

inline std::string HResultMessage(HRESULT result)
{
    char* message = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    FormatMessageA(flags, nullptr, static_cast<DWORD>(result), 0,
                   reinterpret_cast<char*>(&message), 0, nullptr);
    std::string text = message ? message : "erro desconhecido";
    if (message) LocalFree(message);
    return text;
}

inline void ThrowIfFailed(HRESULT result, const char* operation)
{
    if (FAILED(result))
    {
        char code[9]{};
        sprintf_s(code, "%08X", static_cast<unsigned>(result));
        throw std::runtime_error(std::string(operation) + " falhou (HRESULT 0x" +
                                 code + "): " + HResultMessage(result));
    }
}

constexpr UINT64 AlignConstantBufferSize(UINT64 size)
{
    // D3D12 exige que o endereço de uma constant buffer seja múltiplo de 256 bytes.
    return (size + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) &
           ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
}

inline D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource,
                                                 D3D12_RESOURCE_STATES before,
                                                 D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}
