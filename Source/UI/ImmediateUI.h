#pragma once
#include <string>
#include <vector>
#include "../Math/Vector3.h"

// Forward declarations
class DX12Renderer;
struct ID3D12GraphicsCommandList;

struct RaycastResult {
    bool intersected = false;
    float t = -1.0f;
    Vector3 point = Vector3(0.0f, 0.0f, 0.0f);
    float u = -1.0f;
    float v = -1.0f;
    std::string triggeredAction = "none";
};

class ImmediateUI {
public:
    struct ButtonInfo {
        std::string label;
        Vector3 position;
        Vector3 size;
    };

    ImmediateUI();
    ~ImmediateUI();

    bool Initialize(DX12Renderer* renderer);
    void BeginFrame();
    void Text(const std::string& text, const Vector3& position);
    bool Button(const std::string& label, const Vector3& position, const Vector3& size);
    void EndFrame(DX12Renderer* renderer, ID3D12GraphicsCommandList* commandList);
    void Shutdown();

    // Raycast interface for E2E tests
    static RaycastResult RaycastUI(const Vector3& rayOrigin, const Vector3& rayDir);

    // Static registry accessors to support E2E CLI test runs
    static const std::vector<ButtonInfo>& GetRegisteredButtons();
    static void RegisterDefaultButtonsIfEmpty();

private:
    DX12Renderer* m_Renderer;
};
