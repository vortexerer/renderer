#include <windows.h>
#include "Core/Engine.h"
#include "Core/Logger.h"

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    Logger::Info("Starting Game Engine...");
    
    Engine engine;
    if (!engine.Initialize(hInstance, nCmdShow)) {
        Logger::Error("Failed to initialize Game Engine.");
        return -1;
    }

    engine.Run();
    engine.Shutdown();

    Logger::Info("Game Engine exited cleanly.");
    return 0;
}
