#pragma once

#include <Windows.h>

class Application
{
public:
    explicit Application(HINSTANCE instance);
    int Run(int showCommand);

private:
    HINSTANCE instance_ = nullptr;
};

