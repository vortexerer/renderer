#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <vector>
#include "PipelineState.h"
#include "../Platform/OpenXRManager.h" // For XrView, XrSession, XrSwapchain, XrSwapchainCreateInfo

// Forward declarations
class GameObject;

class DX12Renderer {
public:
    DX12Renderer();
    ~DX12Renderer();

    bool Initialize(HWND hWnd);
    void PreRender();
    void RenderScene(const std::vector<XrView>& views, const std::vector<GameObject*>& objects,
                     uint32_t imageIndexL, uint32_t imageIndexR,
                     ID3D12Resource* leftResource, ID3D12Resource* rightResource,
                     uint32_t widthL, uint32_t heightL,
                     uint32_t widthR, uint32_t heightR);
    void PostRender();
    void CreateOpenXRSwapchain(XrSession session, const XrSwapchainCreateInfo& info, XrSwapchain& swapchain, std::vector<ID3D12Resource*>& images);
    void Shutdown();

    void ResetCommandList();
    void BindRenderTargetArray(uint32_t imageIndexL, uint32_t imageIndexR);
    void InitXrRenderTargets(const std::vector<ID3D12Resource*>& leftImages, const std::vector<ID3D12Resource*>& rightImages);
    void ClearRenderTargets();
    void UpdateConstantBuffer(const ConstantBuffer& cbData);
    void DrawSceneInstanced(const std::vector<GameObject*>& objects);
    void SubmitCommands();
    void WaitForGPU();

    ID3D12GraphicsCommandList* GetCommandList() const { return m_CommandList.Get(); }
    ID3D12Device* GetDevice() const { return m_Device.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return m_CommandQueue.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;

    // Swapchain
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_SwapChain;
    static const int FrameCount = 2;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_RenderTargets[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    UINT m_RtvDescriptorSize;
    UINT m_FrameIndex;

    // OpenXR Render Targets
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_XrRtvHeaps[2];
    UINT m_XrRtvDescriptorSize = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView = {};

    PipelineState m_PipelineState;

    // Synchronization
    HANDLE m_FenceEvent;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_Fence;
    UINT64 m_FenceValue;
};
