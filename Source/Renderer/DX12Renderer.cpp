#include "DX12Renderer.h"
#include "../Core/Logger.h"
#include "ConstantBuffer.h"
#include "../Math/Quaternion.h" // viewPose orientation -> rotation matrix
#include <cmath>
#include <cstring>

struct Vertex {
    float position[3];
    float normal[3];
    float texcoord[2];
};

DX12Renderer::DX12Renderer() :
    m_RtvDescriptorSize(0),
    m_FrameIndex(0),
    m_FenceEvent(nullptr),
    m_FenceValue(0) {}

DX12Renderer::~DX12Renderer() {
    Shutdown();
}

bool DX12Renderer::Initialize(HWND hWnd) {
    HRESULT hr = S_OK;

    // 1. Create Device
    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device));
    if (FAILED(hr)) {
        Logger::Error("Failed to create D3D12 physical device. Running in fallback/mock rendering mode.");
        return false;
    }

    // 2. Create Command Queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue));
    if (FAILED(hr)) return false;

    // 3. Create Command Allocator
    hr = m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator));
    if (FAILED(hr)) return false;

    // 4. Create Command List
    hr = m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList));
    if (FAILED(hr)) return false;
    m_CommandList->Close();

    // 5. Create Swapchain
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = 1024;
    swapChainDesc.Height = 768;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    hr = factory->CreateSwapChainForHwnd(
        m_CommandQueue.Get(),
        hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    );
    if (FAILED(hr)) return false;

    hr = swapChain.As(&m_SwapChain);
    if (FAILED(hr)) return false;

    m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

    // 6. Create Descriptor Heaps
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RtvHeap));
    if (FAILED(hr)) return false;

    m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 7. Create Frame Resources
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT n = 0; n < FrameCount; n++) {
        hr = m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_RenderTargets[n]));
        if (FAILED(hr)) return false;
        m_Device->CreateRenderTargetView(m_RenderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_RtvDescriptorSize;
    }

    // 8. Create Sync Objects
    hr = m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
    if (FAILED(hr)) return false;
    m_FenceValue = 1;

    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_FenceEvent) return false;

    // 9. Create Constant Buffer
    {
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeof(ConstantBuffer);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        hr = m_Device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_ConstantBuffer)
        );
        if (SUCCEEDED(hr)) {
            m_ConstantBuffer->Map(0, nullptr, &m_CbMappedData);
        }
    }

    // 10. Create Vertex & Index Buffers
    Vertex vertices[] = {
        // Front Face
        { { -0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f } },
        { {  0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f } },
        { {  0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f } },
        // Back Face
        { { -0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f } },
        { {  0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f } },
        { {  0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f } },
        { { -0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f } },
        // Top Face
        { { -0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f } },
        { {  0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f } },
        { {  0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f } },
        // Bottom Face
        { { -0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f } },
        { { -0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 1.0f } },
        { {  0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f } },
        { {  0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 0.0f } },
        // Left Face
        { { -0.5f, -0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } },
        { { -0.5f, -0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } },
        { { -0.5f,  0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } },
        // Right Face
        { {  0.5f, -0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } },
        { {  0.5f, -0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } },
        { {  0.5f,  0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } },
        { {  0.5f,  0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } }
    };
    uint16_t indices[] = {
        0, 1, 2, 0, 2, 3,       // Front
        4, 5, 6, 4, 6, 7,       // Back
        8, 9, 10, 8, 10, 11,    // Top
        12, 13, 14, 12, 14, 15, // Bottom
        16, 17, 18, 16, 18, 19, // Left
        20, 21, 22, 20, 22, 23  // Right
    };

    {
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeof(vertices);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        hr = m_Device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_VertexBuffer)
        );
        if (SUCCEEDED(hr)) {
            void* pVertexDataBegin = nullptr;
            m_VertexBuffer->Map(0, nullptr, &pVertexDataBegin);
            memcpy(pVertexDataBegin, vertices, sizeof(vertices));
            m_VertexBuffer->Unmap(0, nullptr);

            m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
            m_VertexBufferView.StrideInBytes = sizeof(Vertex);
            m_VertexBufferView.SizeInBytes = sizeof(vertices);
        }
    }

    {
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeof(indices);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        hr = m_Device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_IndexBuffer)
        );
        if (SUCCEEDED(hr)) {
            void* pIndexDataBegin = nullptr;
            m_IndexBuffer->Map(0, nullptr, &pIndexDataBegin);
            memcpy(pIndexDataBegin, indices, sizeof(indices));
            m_IndexBuffer->Unmap(0, nullptr);

            m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
            m_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
            m_IndexBufferView.SizeInBytes = sizeof(indices);
        }
    }

    // 11. Initialize PipelineState
    m_PipelineState.Initialize(m_Device.Get());

    return true;
}

void DX12Renderer::PreRender() {
    ResetCommandList();
}

void DX12Renderer::RenderScene(const std::vector<XrView>& views, const std::vector<GameObject*>& objects,
                               uint32_t imageIndexL, uint32_t imageIndexR,
                               ID3D12Resource* leftResource, ID3D12Resource* rightResource,
                               uint32_t widthL, uint32_t heightL,
                               uint32_t widthR, uint32_t heightR) {
    if (!m_CommandList) return;

    // Transition swapchain targets to render target states
    D3D12_RESOURCE_BARRIER barriers[2] = {};
    if (leftResource) {
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = leftResource;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    }
    if (rightResource) {
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = rightResource;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    }

    UINT barrierCount = 0;
    D3D12_RESOURCE_BARRIER activeBarriers[2];
    if (leftResource) activeBarriers[barrierCount++] = barriers[0];
    if (rightResource) activeBarriers[barrierCount++] = barriers[1];

    if (barrierCount > 0) {
        m_CommandList->ResourceBarrier(barrierCount, activeBarriers);
    }

    // Bind Render Targets
    BindRenderTargetArray(imageIndexL, imageIndexR);

    // Clear render targets
    if (m_XrRtvHeaps[0] && m_XrRtvHeaps[1]) {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandleL = m_XrRtvHeaps[0]->GetCPUDescriptorHandleForHeapStart();
        rtvHandleL.ptr += imageIndexL * m_XrRtvDescriptorSize;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandleR = m_XrRtvHeaps[1]->GetCPUDescriptorHandleForHeapStart();
        rtvHandleR.ptr += imageIndexR * m_XrRtvDescriptorSize;

        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        m_CommandList->ClearRenderTargetView(rtvHandleL, clearColor, 0, nullptr);
        m_CommandList->ClearRenderTargetView(rtvHandleR, clearColor, 0, nullptr);
    }

    // Implement genuine viewport and scissor setup using sizes retrieved from swapchains
    D3D12_VIEWPORT viewports[2] = {};
    viewports[0].TopLeftX = 0.0f;
    viewports[0].TopLeftY = 0.0f;
    viewports[0].Width = static_cast<float>(widthL);
    viewports[0].Height = static_cast<float>(heightL);
    viewports[0].MinDepth = 0.0f;
    viewports[0].MaxDepth = 1.0f;

    viewports[1].TopLeftX = 0.0f;
    viewports[1].TopLeftY = 0.0f;
    viewports[1].Width = static_cast<float>(widthR);
    viewports[1].Height = static_cast<float>(heightR);
    viewports[1].MinDepth = 0.0f;
    viewports[1].MaxDepth = 1.0f;

    m_CommandList->RSSetViewports(2, viewports);

    D3D12_RECT scissorRects[2] = {};
    scissorRects[0].left = 0;
    scissorRects[0].top = 0;
    scissorRects[0].right = static_cast<LONG>(widthL);
    scissorRects[0].bottom = static_cast<LONG>(heightL);

    scissorRects[1].left = 0;
    scissorRects[1].top = 0;
    scissorRects[1].right = static_cast<LONG>(widthR);
    scissorRects[1].bottom = static_cast<LONG>(heightR);

    m_CommandList->RSSetScissorRects(2, scissorRects);

    // Update constant buffer with matrices from left and right eye view (views[0], views[1])
    if (views.size() >= 2) {
        ConstantBuffer cbData = {};

        auto Transpose = [](const Matrix4x4& mat) {
            Matrix4x4 res;
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    res(r, c) = mat(c, r);
                }
            }
            return res;
        };

        for (int i = 0; i < 2; ++i) {
            const XrPosef& viewPose = views[i].pose;
            const XrFovf& fov = views[i].fov;

            // 1. View Matrix Calculation
            Quaternion q(viewPose.orientation.w, viewPose.orientation.x, viewPose.orientation.y, viewPose.orientation.z);
            Matrix4x4 R = q.ToRotationMatrix();

            // Transpose rotation matrix (inverse of orthogonal matrix)
            Matrix4x4 R_T = Matrix4x4::Identity();
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    R_T(row, col) = R(col, row);
                }
            }

            // Translation inversion
            float tx = viewPose.position.x;
            float ty = viewPose.position.y;
            float tz = viewPose.position.z;

            Matrix4x4 viewMat = R_T;
            viewMat(0, 3) = -(R_T(0, 0) * tx + R_T(0, 1) * ty + R_T(0, 2) * tz);
            viewMat(1, 3) = -(R_T(1, 0) * tx + R_T(1, 1) * ty + R_T(1, 2) * tz);
            viewMat(2, 3) = -(R_T(2, 0) * tx + R_T(2, 1) * ty + R_T(2, 2) * tz);
            viewMat(3, 3) = 1.0f;

            // 2. Projection Matrix Calculation (tangent-based, RHS D3D12 depth [0, 1])
            float tanLeft = std::tan(fov.angleLeft);
            float tanRight = std::tan(fov.angleRight);
            float tanUp = std::tan(fov.angleUp);
            float tanDown = std::tan(fov.angleDown);

            float nearVal = 0.05f;
            float farVal = 100.0f;

            float r_minus_l = tanRight - tanLeft;
            float t_minus_b = tanUp - tanDown;
            float f_minus_n = farVal - nearVal;

            Matrix4x4 projMat;
            projMat(0, 0) = 2.0f / r_minus_l;
            projMat(0, 1) = 0.0f;
            projMat(0, 2) = (tanRight + tanLeft) / r_minus_l;
            projMat(0, 3) = 0.0f;

            projMat(1, 0) = 0.0f;
            projMat(1, 1) = 2.0f / t_minus_b;
            projMat(1, 2) = (tanUp + tanDown) / t_minus_b;
            projMat(1, 3) = 0.0f;

            projMat(2, 0) = 0.0f;
            projMat(2, 1) = 0.0f;
            projMat(2, 2) = -farVal / f_minus_n;
            projMat(2, 3) = -(farVal * nearVal) / f_minus_n;

            projMat(3, 0) = 0.0f;
            projMat(3, 1) = 0.0f;
            projMat(3, 2) = -1.0f;
            projMat(3, 3) = 0.0f;

            // Transpose for column-major HLSL
            cbData.View[i] = Transpose(viewMat);
            cbData.Projection[i] = Transpose(projMat);
        }

        cbData.Model = Matrix4x4::Identity(); // Default model identity
        UpdateConstantBuffer(cbData);
    }

    // Draw scene instanced
    DrawSceneInstanced(objects);

    // Transition back to present state
    if (leftResource) {
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    }
    if (rightResource) {
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    }

    barrierCount = 0;
    if (leftResource) activeBarriers[barrierCount++] = barriers[0];
    if (rightResource) activeBarriers[barrierCount++] = barriers[1];

    if (barrierCount > 0) {
        m_CommandList->ResourceBarrier(barrierCount, activeBarriers);
    }
}

void DX12Renderer::PostRender() {
    SubmitCommands();
    if (m_SwapChain) {
        m_SwapChain->Present(1, 0);
    }
    WaitForGPU();
}

void DX12Renderer::CreateOpenXRSwapchain(XrSession session, const XrSwapchainCreateInfo& info, XrSwapchain& swapchain, std::vector<ID3D12Resource*>& images) {
    if (session == XR_NULL_HANDLE) return;

    XrResult result = xrCreateSwapchain(session, &info, &swapchain);
    if (result != XR_SUCCESS) {
        Logger::Error("DX12Renderer: Failed to create OpenXR swapchain.");
        return;
    }

    uint32_t imageCount = 0;
    xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
    if (imageCount > 0) {
        std::vector<XrSwapchainImageD3D12KHR> swapchainImages(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR });
        xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages.data()));
        
        images.resize(imageCount);

        // Setup dynamic descriptor heap for RTV views of swapchain images
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = imageCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RtvHeap));
        m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (uint32_t i = 0; i < imageCount; ++i) {
            images[i] = swapchainImages[i].texture;
            m_Device->CreateRenderTargetView(images[i], nullptr, rtvHandle);
            rtvHandle.ptr += m_RtvDescriptorSize;
        }
    }
}

void DX12Renderer::ResetCommandList() {
    if (m_CommandAllocator && m_CommandList) {
        m_CommandAllocator->Reset();
        m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
    }
}

void DX12Renderer::BindRenderTargetArray(uint32_t imageIndexL, uint32_t imageIndexR) {
    if (!m_CommandList || !m_XrRtvHeaps[0] || !m_XrRtvHeaps[1]) return;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandleL = m_XrRtvHeaps[0]->GetCPUDescriptorHandleForHeapStart();
    rtvHandleL.ptr += imageIndexL * m_XrRtvDescriptorSize;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandleR = m_XrRtvHeaps[1]->GetCPUDescriptorHandleForHeapStart();
    rtvHandleR.ptr += imageIndexR * m_XrRtvDescriptorSize;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = { rtvHandleL, rtvHandleR };
    m_CommandList->OMSetRenderTargets(2, rtvHandles, FALSE, nullptr);
}

void DX12Renderer::InitXrRenderTargets(const std::vector<ID3D12Resource*>& leftImages, const std::vector<ID3D12Resource*>& rightImages) {
    m_XrRtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create RTV heap for Left Eye
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDescL = {};
    rtvHeapDescL.NumDescriptors = static_cast<UINT>(leftImages.size());
    rtvHeapDescL.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDescL.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_Device->CreateDescriptorHeap(&rtvHeapDescL, IID_PPV_ARGS(&m_XrRtvHeaps[0]));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandleL = m_XrRtvHeaps[0]->GetCPUDescriptorHandleForHeapStart();
    for (size_t i = 0; i < leftImages.size(); ++i) {
        m_Device->CreateRenderTargetView(leftImages[i], nullptr, rtvHandleL);
        rtvHandleL.ptr += m_XrRtvDescriptorSize;
    }

    // Create RTV heap for Right Eye
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDescR = {};
    rtvHeapDescR.NumDescriptors = static_cast<UINT>(rightImages.size());
    rtvHeapDescR.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDescR.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_Device->CreateDescriptorHeap(&rtvHeapDescR, IID_PPV_ARGS(&m_XrRtvHeaps[1]));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandleR = m_XrRtvHeaps[1]->GetCPUDescriptorHandleForHeapStart();
    for (size_t i = 0; i < rightImages.size(); ++i) {
        m_Device->CreateRenderTargetView(rightImages[i], nullptr, rtvHandleR);
        rtvHandleR.ptr += m_XrRtvDescriptorSize;
    }
}

void DX12Renderer::ClearRenderTargets() {
    if (!m_CommandList || !m_SwapChain) return;
    
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_RenderTargets[m_FrameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_CommandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += m_FrameIndex * m_RtvDescriptorSize;
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

void DX12Renderer::UpdateConstantBuffer(const ConstantBuffer& cbData) {
    if (m_CbMappedData) {
        memcpy(m_CbMappedData, &cbData, sizeof(ConstantBuffer));
    }
}

void DX12Renderer::DrawSceneInstanced(const std::vector<GameObject*>& objects) {
    if (!m_CommandList) return;

    // Bind Root Signature and Pipeline State
    m_CommandList->SetGraphicsRootSignature(m_PipelineState.GetRootSignature());
    m_CommandList->SetPipelineState(m_PipelineState.GetPSO());

    // Bind Constant Buffer
    if (m_ConstantBuffer) {
        m_CommandList->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer->GetGPUVirtualAddress());
    }

    // Bind Vertex and Index Buffers
    m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
    m_CommandList->IASetIndexBuffer(&m_IndexBufferView);

    // Draw 2 instances of each object (stereoscopic views mapping)
    for (auto* obj : objects) {
        (void)obj;
        // Draw 36 indices, 2 instances
        m_CommandList->DrawIndexedInstanced(36, 2, 0, 0, 0);
    }
}

void DX12Renderer::SubmitCommands() {
    if (!m_CommandList || !m_CommandQueue) return;

    m_CommandList->Close();
    ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

void DX12Renderer::WaitForGPU() {
    if (!m_CommandQueue || !m_Fence) return;

    const UINT64 fence = m_FenceValue;
    m_CommandQueue->Signal(m_Fence.Get(), fence);
    m_FenceValue++;

    if (m_Fence->GetCompletedValue() < fence) {
        m_Fence->SetEventOnCompletion(fence, m_FenceEvent);
        WaitForSingleObject(m_FenceEvent, INFINITE);
    }

    if (m_SwapChain) {
        m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
    }
}

void DX12Renderer::Shutdown() {
    if (m_ConstantBuffer) {
        m_ConstantBuffer->Unmap(0, nullptr);
        m_ConstantBuffer.Reset();
    }
    m_CbMappedData = nullptr;

    m_VertexBuffer.Reset();
    m_IndexBuffer.Reset();
    m_PipelineState.Shutdown();

    m_XrRtvHeaps[0].Reset();
    m_XrRtvHeaps[1].Reset();

    if (m_FenceEvent) {
        CloseHandle(m_FenceEvent);
        m_FenceEvent = nullptr;
    }
    m_Fence.Reset();
    m_RtvHeap.Reset();
    for (int i = 0; i < FrameCount; i++) {
        m_RenderTargets[i].Reset();
    }
    m_SwapChain.Reset();
    m_CommandList.Reset();
    m_CommandAllocator.Reset();
    m_CommandQueue.Reset();
    m_Device.Reset();
}
