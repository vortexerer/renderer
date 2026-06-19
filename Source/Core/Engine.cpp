#include "Engine.h"
#include "Logger.h"

Engine::Engine() {}

Engine::~Engine() {
    Shutdown();
}

bool Engine::Initialize(HINSTANCE hInstance, int nCmdShow) {
    Logger::Info("Engine: Initializing subsystems...");

    if (!m_Window.Initialize(hInstance, nCmdShow, 1024, 768, L"VR Target Shooter Game")) {
        Logger::Error("Engine: Failed to initialize Win32 Window.");
        return false;
    }

    if (!m_XRManager.Initialize()) {
        Logger::Error("Engine: Failed to initialize OpenXR Manager.");
        return false;
    }

    if (!m_Renderer.Initialize(m_Window.GetHWND())) {
        Logger::Error("Engine: Failed to initialize DX12 Renderer.");
        return false;
    }

    if (!m_XRManager.InitializeSession(m_Renderer.GetDevice(), m_Renderer.GetCommandQueue())) {
        Logger::Error("Engine: Failed to initialize OpenXR Session.");
        return false;
    }

    m_Renderer.InitXrRenderTargets(
        m_XRManager.GetSwapchainImages(LeftEye),
        m_XRManager.GetSwapchainImages(RightEye)
    );

    if (!m_Audio.Initialize()) {
        Logger::Error("Engine: Failed to initialize Audio Engine.");
        return false;
    }

    m_GameWorld.LoadLevel("Assets/Models/Level.gltf");
    Logger::Info("Engine: All subsystems initialized successfully.");
    return true;
}

void Engine::Run() {
    m_Timer.Tick();
    double lastTime = m_Timer.GetTotalTime();

    while (m_Window.IsRunning()) {
        m_Window.PollOSMessages();
        m_XRManager.PollEvents();

        if (m_XRManager.IsSessionRunning()) {
            XrFrameState frameState = { XR_TYPE_FRAME_STATE };
            m_XRManager.WaitFrame(&frameState);
            m_XRManager.BeginFrame();

            m_Timer.Tick();
            double currentTime = m_Timer.GetTotalTime();
            double frameTime = currentTime - lastTime;
            lastTime = currentTime;

            // Update Controller Inputs & Player Controller
            m_GameWorld.UpdateInput(&m_XRManager, frameState.predictedDisplayTime);

            // Update Physics (Accumulator based 100Hz loop inside PhysicsEngine)
            m_Physics.Update(frameTime);

            // Update Audio
            m_Audio.Update(static_cast<float>(frameTime));

            // Locate views for stereoscopic rendering
            std::vector<XrView> views;
            m_XRManager.LocateViews(m_XRManager.GetPlaySpace(), frameState.predictedDisplayTime, views);

            uint32_t imageIndexL = 0;
            uint32_t imageIndexR = 0;
            m_XRManager.AcquireAndWaitSwapchainImage(LeftEye, &imageIndexL);
            m_XRManager.AcquireAndWaitSwapchainImage(RightEye, &imageIndexR);

            // Stereo rendering pass
            m_Renderer.PreRender();
            m_Renderer.RenderScene(views, m_GameWorld.GetGameObjects(),
                                   imageIndexL, imageIndexR,
                                   m_XRManager.GetSwapchainImages(LeftEye)[imageIndexL],
                                   m_XRManager.GetSwapchainImages(RightEye)[imageIndexR],
                                   m_XRManager.GetSwapchainWidth(LeftEye), m_XRManager.GetSwapchainHeight(LeftEye),
                                   m_XRManager.GetSwapchainWidth(RightEye), m_XRManager.GetSwapchainHeight(RightEye));
            m_Renderer.PostRender();

            m_XRManager.ReleaseSwapchainImage(LeftEye);
            m_XRManager.ReleaseSwapchainImage(RightEye);

            m_XRManager.EndFrame(frameState.predictedDisplayTime, views);
        } else {
            m_Timer.Tick();
            double currentTime = m_Timer.GetTotalTime();
            double frameTime = currentTime - lastTime;
            lastTime = currentTime;

            m_Physics.Update(frameTime);
            m_Audio.Update(static_cast<float>(frameTime));
            
            Sleep(10); // Throttle loop
        }
    }
}

void Engine::Shutdown() {
    Logger::Info("Engine: Shutting down subsystems...");
    m_Audio.Shutdown();
    m_Renderer.Shutdown();
    m_XRManager.Shutdown();
    m_Window.Shutdown();
}
