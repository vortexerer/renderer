#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class PipelineState {
public:
    PipelineState();
    ~PipelineState();

    bool Initialize(ID3D12Device* device);
    void Shutdown();

    ID3D12PipelineState* GetPSO() const { return m_PipelineState.Get(); }
    ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
};
