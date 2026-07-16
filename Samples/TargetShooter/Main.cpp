#include <windows.h>
#include "Core/Engine.h"
#include "Core/Logger.h"
#include "GameWorld.h"

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    Logger::Info("Starting Game Engine...");
    
    GameWorld game;
    Engine engine;
    if (!engine.Initialize(hInstance, nCmdShow, &game)) {
        Logger::Error("Failed to initialize Game Engine.");
        return -1;
    }

    engine.Run();
    engine.Shutdown();

    Logger::Info("Game Engine exited cleanly.");
    return 0;
}
