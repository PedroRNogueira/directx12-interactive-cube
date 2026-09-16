#include "Application.h"

#include <Windows.h>
#include <exception>
#include <string>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    try
    {
        Application application(instance);
        return application.Run(showCommand);
    }
    catch (const std::exception& error)
    {
        const std::string text = error.what();
        MessageBoxA(nullptr, text.c_str(), "Falha ao iniciar DirectX 12", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
}

