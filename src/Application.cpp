#include "Application.h"

Application::Application(HINSTANCE instance) : instance_(instance) {}

int Application::Run(int)
{
    // A janela e o loop principal entram no proximo incremento do projeto.
    return instance_ != nullptr ? 0 : 1;
}

