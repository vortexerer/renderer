#pragma once
#include <windows.h>
#include "../Platform/Win32Window.h"
#include "../Platform/OpenXRManager.h"
#include "../Renderer/DX12Renderer.h"
#include "../Physics/PhysicsEngine.h"
#include "../Audio/AudioEngine.h"
#include "IGame.h"
#include "Timer.h"

class Engine {
public:
    Engine();
    ~Engine();

    // game은 앱(샘플)이 소유하며, 엔진 수명 동안 유효해야 합니다. nullptr이면
    // 게임 로직 없이 서브시스템만 구동합니다.
    bool Initialize(HINSTANCE hInstance, int nCmdShow, IGame* game = nullptr);
    void Run();
    void Shutdown();

private:
    Win32Window m_Window;
    OpenXRManager m_XRManager;
    DX12Renderer m_Renderer;
    PhysicsEngine m_Physics;
    AudioEngine m_Audio;
    IGame* m_Game = nullptr;
    Timer m_Timer;
};
