#pragma once
#include <vector>
#include <d3d12.h>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

enum Eye {
    LeftEye = 0,
    RightEye = 1
};

class OpenXRManager {
public:
    OpenXRManager();
    ~OpenXRManager();

    bool Initialize();
    bool InitializeSession(ID3D12Device* device, ID3D12CommandQueue* queue);
    bool CreateSession(ID3D12Device* device, ID3D12CommandQueue* queue);
    void PollEvents();
    void WaitFrame(XrFrameState* frameState);
    void BeginFrame();
    void EndFrame(XrTime predictedDisplayTime, const std::vector<XrView>& views);
    void Shutdown();

    bool IsSessionRunning() const { return m_SessionRunning; }

    XrSpace GetPlaySpace() const { return m_PlaySpace; }
    XrSession GetSession() const { return m_Session; }
    
    void LocateViews(XrSpace playSpace, XrTime predictedDisplayTime, std::vector<XrView>& views);
    void AcquireAndWaitSwapchainImage(Eye eye, uint32_t* imageIndex);
    void ReleaseSwapchainImage(Eye eye);

    void CreateOpenXRSwapchain(XrSession session, const XrSwapchainCreateInfo& info, XrSwapchain& swapchain, std::vector<ID3D12Resource*>& images);

    // Tracked input poses
    XrPosef GetControllerPose(int handIndex) const;
    bool IsTriggerPressed(int handIndex) const;

    // Retrieve swapchain resources and info
    ID3D12Resource* GetSwapchainResource(Eye eye, uint32_t imageIndex) const {
        if (imageIndex < m_SwapchainImages[eye].size()) {
            return m_SwapchainImages[eye][imageIndex];
        }
        return nullptr;
    }
    void GetSwapchainSize(Eye eye, uint32_t& width, uint32_t& height) const {
        width = m_SwapchainWidth[eye];
        height = m_SwapchainHeight[eye];
    }
    uint32_t GetSwapchainWidth(Eye eye) const { return m_SwapchainWidth[eye]; }
    uint32_t GetSwapchainHeight(Eye eye) const { return m_SwapchainHeight[eye]; }
    const std::vector<ID3D12Resource*>& GetSwapchainImages(Eye eye) const { return m_SwapchainImages[eye]; }

private:
    XrInstance m_Instance = XR_NULL_HANDLE;
    XrSystemId m_SystemId = XR_NULL_SYSTEM_ID;
    XrSession m_Session = XR_NULL_HANDLE;
    XrSpace m_PlaySpace = XR_NULL_HANDLE;
    
    // Actions & Input tracking
    XrActionSet m_ActionSet = XR_NULL_HANDLE;
    XrAction m_PoseAction = XR_NULL_HANDLE;
    XrAction m_TriggerAction = XR_NULL_HANDLE;
    XrSpace m_HandSpaces[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };

    XrPath m_HandSubpaths[2] = { XR_NULL_PATH, XR_NULL_PATH };
    XrSwapchain m_Swapchains[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
    std::vector<ID3D12Resource*> m_SwapchainImages[2];
    uint32_t m_SwapchainWidth[2] = { 1024, 1024 };
    uint32_t m_SwapchainHeight[2] = { 768, 768 };
    bool m_SessionRunning = false;
};
