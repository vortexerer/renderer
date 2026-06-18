#include "OpenXRManager.h"
#include "../Core/Logger.h"
#include <cstring>
#include <iostream>

OpenXRManager::OpenXRManager() {}

OpenXRManager::~OpenXRManager() {
    Shutdown();
}

bool OpenXRManager::Initialize() {
    // 1. Create OpenXR Instance
    XrInstanceCreateInfo instanceCreateInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    strcpy_s(instanceCreateInfo.applicationInfo.applicationName, "VR Target Shooter");
    instanceCreateInfo.applicationInfo.applicationVersion = 1;
    strcpy_s(instanceCreateInfo.applicationInfo.engineName, "GameEngineDEV");
    instanceCreateInfo.applicationInfo.engineVersion = 1;
    instanceCreateInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    // Enable D3D12 extension
    std::vector<const char*> extensions = { XR_KHR_D3D12_ENABLE_EXTENSION_NAME };
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceCreateInfo.enabledExtensionNames = extensions.data();

    XrResult result = xrCreateInstance(&instanceCreateInfo, &m_Instance);
    if (result != XR_SUCCESS) {
        Logger::Error("OpenXRManager: Failed to create OpenXR instance.");
        return false;
    }

    // 2. Get HMD System
    XrSystemGetInfo systemGetInfo = { XR_TYPE_SYSTEM_GET_INFO };
    systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    result = xrGetSystem(m_Instance, &systemGetInfo, &m_SystemId);
    if (result != XR_SUCCESS) {
        Logger::Error("OpenXRManager: Failed to get HMD system.");
        return false;
    }

    // 3. Create Action Set & Actions for Controllers
    XrActionSetCreateInfo actionSetInfo = { XR_TYPE_ACTION_SET_CREATE_INFO };
    strcpy_s(actionSetInfo.actionSetName, "gameplay");
    strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
    actionSetInfo.priority = 0;
    result = xrCreateActionSet(m_Instance, &actionSetInfo, &m_ActionSet);
    if (result != XR_SUCCESS) {
        Logger::Error("OpenXRManager: Failed to create ActionSet.");
        return false;
    }

    // Hand pose action
    XrActionCreateInfo poseActionInfo = { XR_TYPE_ACTION_CREATE_INFO };
    strcpy_s(poseActionInfo.actionName, "hand_pose");
    poseActionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    strcpy_s(poseActionInfo.localizedActionName, "Hand Pose");
    
    xrStringToPath(m_Instance, "/user/hand/left", &m_HandSubpaths[0]);
    xrStringToPath(m_Instance, "/user/hand/right", &m_HandSubpaths[1]);
    
    poseActionInfo.countSubactionPaths = 2;
    poseActionInfo.subactionPaths = m_HandSubpaths;
    result = xrCreateAction(m_ActionSet, &poseActionInfo, &m_PoseAction);
    if (result != XR_SUCCESS) {
        Logger::Error("OpenXRManager: Failed to create Hand Pose action.");
        return false;
    }

    // Trigger press action
    XrActionCreateInfo triggerActionInfo = { XR_TYPE_ACTION_CREATE_INFO };
    strcpy_s(triggerActionInfo.actionName, "trigger_press");
    triggerActionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strcpy_s(triggerActionInfo.localizedActionName, "Trigger Press");
    triggerActionInfo.countSubactionPaths = 2;
    triggerActionInfo.subactionPaths = m_HandSubpaths;
    result = xrCreateAction(m_ActionSet, &triggerActionInfo, &m_TriggerAction);
    if (result != XR_SUCCESS) {
        Logger::Error("OpenXRManager: Failed to create Trigger action.");
        return false;
    }

    Logger::Info("OpenXRManager: OpenXR instance and action configurations initialized successfully.");
    return true;
}

bool OpenXRManager::InitializeSession(ID3D12Device* device, ID3D12CommandQueue* queue) {
    // 1. Get D3D12 graphics requirements
    PFN_xrGetD3D12GraphicsRequirementsKHR pfnGetD3D12GraphicsRequirementsKHR = nullptr;
    XrResult result = xrGetInstanceProcAddr(m_Instance, "xrGetD3D12GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnGetD3D12GraphicsRequirementsKHR);
    if (result != XR_SUCCESS || !pfnGetD3D12GraphicsRequirementsKHR) {
        Logger::Error("OpenXRManager: Failed to get xrGetD3D12GraphicsRequirementsKHR function pointer.");
        return false;
    }

    XrGraphicsRequirementsD3D12KHR graphicsRequirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR };
    result = pfnGetD3D12GraphicsRequirementsKHR(m_Instance, m_SystemId, &graphicsRequirements);
    if (result != XR_SUCCESS) {
        Logger::Error("OpenXRManager: Failed to get D3D12 graphics requirements.");
        return false;
    }

    // 2. Create Session with Graphics Binding
    XrGraphicsBindingD3D12KHR graphicsBinding = { XR_TYPE_GRAPHICS_BINDING_D3D12_KHR };
    graphicsBinding.device = device;
    graphicsBinding.queue = queue;

    XrSessionCreateInfo sessionCreateInfo = { XR_TYPE_SESSION_CREATE_INFO };
    sessionCreateInfo.next = &graphicsBinding;
    sessionCreateInfo.systemId = m_SystemId;

    result = xrCreateSession(m_Instance, &sessionCreateInfo, &m_Session);
    if (result != XR_SUCCESS) {
        Logger::Error("OpenXRManager: Failed to create OpenXR session.");
        return false;
    }

    // 3. Create Reference Space (Stage falling back to Local)
    XrReferenceSpaceCreateInfo spaceCreateInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    spaceCreateInfo.poseInReferenceSpace.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
    spaceCreateInfo.poseInReferenceSpace.position = { 0.0f, 0.0f, 0.0f };

    result = xrCreateReferenceSpace(m_Session, &spaceCreateInfo, &m_PlaySpace);
    if (result != XR_SUCCESS) {
        Logger::Info("OpenXRManager: Stage reference space unsupported, falling back to Local.");
        spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        result = xrCreateReferenceSpace(m_Session, &spaceCreateInfo, &m_PlaySpace);
        if (result != XR_SUCCESS) {
            Logger::Error("OpenXRManager: Failed to create reference space.");
            return false;
        }
    }

    // 4. Suggest simple controller profile bindings
    XrPath interactionProfilePath;
    result = xrStringToPath(m_Instance, "/interaction_profiles/khr/simple_controller", &interactionProfilePath);
    if (result != XR_SUCCESS) return false;

    XrPath leftGripPosePath, leftSelectClickPath;
    XrPath rightGripPosePath, rightSelectClickPath;
    xrStringToPath(m_Instance, "/user/hand/left/input/grip/pose", &leftGripPosePath);
    xrStringToPath(m_Instance, "/user/hand/left/input/select/click", &leftSelectClickPath);
    xrStringToPath(m_Instance, "/user/hand/right/input/grip/pose", &rightGripPosePath);
    xrStringToPath(m_Instance, "/user/hand/right/input/select/click", &rightSelectClickPath);

    std::vector<XrActionSuggestedBinding> bindings = {
        { m_PoseAction, leftGripPosePath },
        { m_TriggerAction, leftSelectClickPath },
        { m_PoseAction, rightGripPosePath },
        { m_TriggerAction, rightSelectClickPath }
    };

    XrInteractionProfileSuggestedBindings suggestedBindings = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDINGS };
    suggestedBindings.interactionProfile = interactionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = static_cast<uint32_t>(bindings.size());

    result = xrSuggestInteractionProfileBindings(m_Instance, &suggestedBindings);
    if (result != XR_SUCCESS) {
        Logger::Error("OpenXRManager: Failed to suggest interaction profile bindings.");
        return false;
    }

    // 5. Attach Session Action Sets
    XrSessionActionSetsAttachInfo attachInfo = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &m_ActionSet;

    result = xrAttachSessionActionSets(m_Session, &attachInfo);
    if (result != XR_SUCCESS) {
        Logger::Error("OpenXRManager: Failed to attach session action sets.");
        return false;
    }

    // 6. Create Action Spaces
    for (int i = 0; i < 2; ++i) {
        XrActionSpaceCreateInfo actionSpaceInfo = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
        actionSpaceInfo.action = m_PoseAction;
        actionSpaceInfo.subactionPath = m_HandSubpaths[i];
        actionSpaceInfo.poseInActionSpace.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
        actionSpaceInfo.poseInActionSpace.position = { 0.0f, 0.0f, 0.0f };

        result = xrCreateActionSpace(m_Session, &actionSpaceInfo, &m_HandSpaces[i]);
        if (result != XR_SUCCESS) {
            Logger::Error("OpenXRManager: Failed to create action space for hand.");
            return false;
        }
    }

    // 7. Enumerate View Configuration Views to set up Swapchains
    uint32_t viewCount = 0;
    result = xrEnumerateViewConfigurationViews(m_Instance, m_SystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    if (result != XR_SUCCESS || viewCount < 2) {
        Logger::Error("OpenXRManager: Failed to get view configuration views count.");
        return false;
    }

    std::vector<XrViewConfigurationView> configViews(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
    result = xrEnumerateViewConfigurationViews(m_Instance, m_SystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, configViews.data());
    if (result != XR_SUCCESS) {
        Logger::Error("OpenXRManager: Failed to enumerate view configuration views.");
        return false;
    }

    for (int i = 0; i < 2; ++i) {
        m_SwapchainWidth[i] = configViews[i].recommendedImageRectWidth;
        m_SwapchainHeight[i] = configViews[i].recommendedImageRectHeight;

        XrSwapchainCreateInfo swapchainCreateInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapchainCreateInfo.width = m_SwapchainWidth[i];
        swapchainCreateInfo.height = m_SwapchainHeight[i];
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.sampleCount = configViews[i].recommendedSwapchainSampleCount;
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

        CreateOpenXRSwapchain(m_Session, swapchainCreateInfo, m_Swapchains[i], m_SwapchainImages[i]);
        if (m_Swapchains[i] == XR_NULL_HANDLE) {
            Logger::Error("OpenXRManager: Failed to create swapchain.");
            return false;
        }
    }

    return true;
}

bool OpenXRManager::CreateSession(ID3D12Device* device, ID3D12CommandQueue* queue) {
    return InitializeSession(device, queue);
}

void OpenXRManager::PollEvents() {
    if (m_Instance == XR_NULL_HANDLE) return;

    XrEventDataBuffer eventBuffer = { XR_TYPE_EVENT_DATA_BUFFER };
    while (xrPollEvent(m_Instance, &eventBuffer) == XR_SUCCESS) {
        switch (eventBuffer.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                XrEventDataSessionStateChanged* sessionEvent = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventBuffer);
                if (sessionEvent->state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo beginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
                    beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    xrBeginSession(m_Session, &beginInfo);
                    m_SessionRunning = true;
                    Logger::Info("OpenXRManager: Session started.");
                } else if (sessionEvent->state == XR_SESSION_STATE_STOPPING) {
                    xrEndSession(m_Session);
                    m_SessionRunning = false;
                    Logger::Info("OpenXRManager: Session stopped.");
                }
                break;
            }
            default:
                break;
        }
        eventBuffer.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
}

void OpenXRManager::WaitFrame(XrFrameState* frameState) {
    XrFrameWaitInfo waitInfo = { XR_TYPE_FRAME_WAIT_INFO };
    xrWaitFrame(m_Session, &waitInfo, frameState);
}

void OpenXRManager::BeginFrame() {
    XrFrameBeginInfo beginInfo = { XR_TYPE_FRAME_BEGIN_INFO };
    xrBeginFrame(m_Session, &beginInfo);
}

void OpenXRManager::EndFrame(XrTime predictedDisplayTime, const std::vector<XrView>& views) {
    XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO };
    endInfo.displayTime = predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    
    // Construct projection layer
    XrCompositionLayerProjection projectionLayer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    projectionLayer.layerFlags = XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    projectionLayer.space = m_PlaySpace;

    std::vector<XrCompositionLayerProjectionView> projectionViews(views.size());
    for (size_t i = 0; i < views.size(); ++i) {
        projectionViews[i] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
        projectionViews[i].pose = views[i].pose;
        projectionViews[i].fov = views[i].fov;
        // In real rendering, we also set subImage viewport & swapchain here
    }

    projectionLayer.viewCount = static_cast<uint32_t>(projectionViews.size());
    projectionLayer.views = projectionViews.data();

    const XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer)
    };
    endInfo.layerCount = 1;
    endInfo.layers = layers;

    xrEndFrame(m_Session, &endInfo);
}

void OpenXRManager::Shutdown() {
    for (int i = 0; i < 2; ++i) {
        if (m_HandSpaces[i] != XR_NULL_HANDLE) {
            xrDestroySpace(m_HandSpaces[i]);
            m_HandSpaces[i] = XR_NULL_HANDLE;
        }
    }
    for (int i = 0; i < 2; ++i) {
        if (m_Swapchains[i] != XR_NULL_HANDLE) {
            xrDestroySwapchain(m_Swapchains[i]);
            m_Swapchains[i] = XR_NULL_HANDLE;
        }
        m_SwapchainImages[i].clear();
    }
    if (m_PoseAction != XR_NULL_HANDLE) {
        xrDestroyAction(m_PoseAction);
        m_PoseAction = XR_NULL_HANDLE;
    }
    if (m_TriggerAction != XR_NULL_HANDLE) {
        xrDestroyAction(m_TriggerAction);
        m_TriggerAction = XR_NULL_HANDLE;
    }
    if (m_ActionSet != XR_NULL_HANDLE) {
        xrDestroyActionSet(m_ActionSet);
        m_ActionSet = XR_NULL_HANDLE;
    }
    if (m_PlaySpace != XR_NULL_HANDLE) {
        xrDestroySpace(m_PlaySpace);
        m_PlaySpace = XR_NULL_HANDLE;
    }
    if (m_Session != XR_NULL_HANDLE) {
        xrDestroySession(m_Session);
        m_Session = XR_NULL_HANDLE;
    }
    if (m_Instance != XR_NULL_HANDLE) {
        xrDestroyInstance(m_Instance);
        m_Instance = XR_NULL_HANDLE;
    }
    m_SessionRunning = false;
}

void OpenXRManager::LocateViews(XrSpace playSpace, XrTime predictedDisplayTime, std::vector<XrView>& views) {
    XrViewLocateInfo locateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
    locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locateInfo.displayTime = predictedDisplayTime;
    locateInfo.space = playSpace;

    XrViewState viewState = { XR_TYPE_VIEW_STATE };
    uint32_t viewCountOutput = 0;
    
    // First query view count
    xrLocateViews(m_Session, &locateInfo, &viewState, 0, &viewCountOutput, nullptr);
    if (viewCountOutput > 0) {
        views.resize(viewCountOutput, { XR_TYPE_VIEW });
        xrLocateViews(m_Session, &locateInfo, &viewState, viewCountOutput, &viewCountOutput, views.data());
    }
}

void OpenXRManager::AcquireAndWaitSwapchainImage(Eye eye, uint32_t* imageIndex) {
    if (m_Swapchains[eye] == XR_NULL_HANDLE) {
        if (imageIndex) *imageIndex = 0;
        return;
    }

    XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    uint32_t index = 0;
    XrResult result = xrAcquireSwapchainImage(m_Swapchains[eye], &acquireInfo, &index);
    if (result == XR_SUCCESS) {
        XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
        waitInfo.timeout = XR_INFINITE_DURATION;
        xrWaitSwapchainImage(m_Swapchains[eye], &waitInfo);
        if (imageIndex) {
            *imageIndex = index;
        }
    } else {
        if (imageIndex) *imageIndex = 0;
    }
}

void OpenXRManager::ReleaseSwapchainImage(Eye eye) {
    if (m_Swapchains[eye] == XR_NULL_HANDLE) {
        return;
    }

    XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    xrReleaseSwapchainImage(m_Swapchains[eye], &releaseInfo);
}

void OpenXRManager::CreateOpenXRSwapchain(XrSession session, const XrSwapchainCreateInfo& info, XrSwapchain& swapchain, std::vector<ID3D12Resource*>& images) {
    XrResult result = xrCreateSwapchain(session, &info, &swapchain);
    if (result != XR_SUCCESS) {
        Logger::Error("OpenXRManager: Failed to create swapchain.");
        return;
    }

    uint32_t imageCount = 0;
    xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
    if (imageCount > 0) {
        std::vector<XrSwapchainImageD3D12KHR> swapchainImages(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR });
        xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages.data()));
        
        images.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            images[i] = swapchainImages[i].texture;
        }
    }
}

XrPosef OpenXRManager::GetControllerPose(int handIndex) const {
    XrPosef pose = {};
    pose.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
    pose.position = { 0.0f, 1.5f, -0.5f }; // Fallback defaults

    // In a real loop, sync actions happens before query
    XrSpaceLocation location = { XR_TYPE_SPACE_LOCATION };
    XrResult result = xrLocateSpace(m_HandSpaces[handIndex], m_PlaySpace, 0, &location);
    if (result == XR_SUCCESS && (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
        pose = location.pose;
    }

    return pose;
}

bool OpenXRManager::IsTriggerPressed(int handIndex) const {
    // In a real framework, call xrSyncActions first.
    XrActionStateGetInfo getInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = m_TriggerAction;
    getInfo.subactionPath = m_HandSubpaths[handIndex];

    XrActionStateBoolean triggerState = { XR_TYPE_ACTION_STATE_BOOLEAN };
    XrResult result = xrGetActionStateBoolean(m_Session, &getInfo, &triggerState);
    if (result == XR_SUCCESS && triggerState.isActive) {
        return triggerState.currentState == XR_TRUE;
    }

    return false;
}
