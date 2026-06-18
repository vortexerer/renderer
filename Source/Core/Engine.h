#pragma once
#include <windows.h>
#include "../Platform/Win32Window.h"
#include "../Platform/OpenXRManager.h"
#include "../Renderer/DX12Renderer.h"
#include "../Physics/PhysicsEngine.h"
#include "../Audio/AudioEngine.h"
#include "../Game/GameWorld.h"
#include "Timer.h"

class Engine {
public:
    Engine();
    ~Engine();

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    void Run();
    void Shutdown();

private:
    Win32Window m_Window;
    OpenXRManager m_XRManager;
    DX12Renderer m_Renderer;
    PhysicsEngine m_Physics;
    AudioEngine m_Audio;
    GameWorld m_GameWorld;
    Timer m_Timer;
};
