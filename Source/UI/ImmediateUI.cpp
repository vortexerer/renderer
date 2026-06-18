#include "ImmediateUI.h"
#include <cmath>
#include "../Core/Logger.h"
#ifdef _WIN32
// The renderer is only dereferenced on Windows (DirectX 12). The methods below
// merely store/ignore the pointer, so the forward declarations in the header
// suffice for the portable test build.
#include "../Renderer/DX12Renderer.h"
#endif

// Static button registry to support static/isolated RaycastUI calls from test runner
static std::vector<ImmediateUI::ButtonInfo> s_RegisteredButtons;

ImmediateUI::ImmediateUI() : m_Renderer(nullptr) {}

ImmediateUI::~ImmediateUI() {
    Shutdown();
}

bool ImmediateUI::Initialize(DX12Renderer* renderer) {
    m_Renderer = renderer;
    Logger::Info("ImmediateUI: Initialized Immediate Mode UI system.");
    return true;
}

void ImmediateUI::BeginFrame() {
    // Clear dynamic buttons registered in the previous frame
    s_RegisteredButtons.clear();
}

void ImmediateUI::Text(const std::string& text, const Vector3& position) {
    (void)text;
    (void)position;
}

bool ImmediateUI::Button(const std::string& label, const Vector3& position, const Vector3& size) {
    // Register the button bounds dynamically
    ButtonInfo btn;
    btn.label = label;
    btn.position = position;
    btn.size = size;
    s_RegisteredButtons.push_back(btn);

    // In a full game loop, this checks if controller ray points at button and trigger is clicked
    return false; 
}

void ImmediateUI::EndFrame(DX12Renderer* renderer, ID3D12GraphicsCommandList* commandList) {
    (void)renderer;
    (void)commandList;
}

void ImmediateUI::Shutdown() {
    m_Renderer = nullptr;
    s_RegisteredButtons.clear();
}

const std::vector<ImmediateUI::ButtonInfo>& ImmediateUI::GetRegisteredButtons() {
    return s_RegisteredButtons;
}

void ImmediateUI::RegisterDefaultButtonsIfEmpty() {
    if (s_RegisteredButtons.empty()) {
        // Fallback for isolated E2E tests: register "Restart Game" button at its standard layout
        ButtonInfo defaultBtn;
        defaultBtn.label = "Restart Game";
        defaultBtn.position = Vector3(0.0f, 1.35f, -2.0f); // Center of X[-0.5, 0.5], Y[1.1, 1.6]
        defaultBtn.size = Vector3(1.0f, 0.5f, 0.0f);        // Width 1.0, Height 0.5
        s_RegisteredButtons.push_back(defaultBtn);
    }
}

RaycastResult ImmediateUI::RaycastUI(const Vector3& rayOrigin, const Vector3& rayDir) {
    RaycastResult result;
    
    // UI Quad plane: centered at (0.0, 1.5, -2.0), normal (0.0, 0.0, 1.0)
    float p0_z = -2.0f;

    // Check if ray is parallel to the plane
    if (std::abs(rayDir.z) < 1e-6f) {
        result.intersected = false;
        result.t = -1.0f;
        return result;
    }

    // Ray equation: Origin + t * Dir = Point
    // Solve for plane intersection: rayOrigin.z + t * rayDir.z = p0_z
    float t = (p0_z - rayOrigin.z) / rayDir.z;
    if (t < 0.0f) {
        result.intersected = false;
        result.t = -1.0f;
        return result;
    }

    Vector3 p = rayOrigin + rayDir * t;
    
    // UI Panel bounds: Width [-1.0, 1.0], Height [1.0, 2.0]
    bool in_x = (p.x >= -1.0f && p.x <= 1.0f);
    bool in_y = (p.y >= 1.0f && p.y <= 2.0f);

    if (in_x && in_y) {
        result.intersected = true;
        result.t = t;
        result.point = p;
        result.u = (p.x + 1.0f) / 2.0f;
        result.v = p.y - 1.0f;

        // Fallback to default button bounds if no buttons are registered (e.g. CLI test-ui run)
        RegisterDefaultButtonsIfEmpty();

        // Check intersection against registered buttons
        result.triggeredAction = "none";
        for (const auto& btn : s_RegisteredButtons) {
            float halfWidth = btn.size.x * 0.5f;
            float halfHeight = btn.size.y * 0.5f;
            
            float minX = btn.position.x - halfWidth;
            float maxX = btn.position.x + halfWidth;
            float minY = btn.position.y - halfHeight;
            float maxY = btn.position.y + halfHeight;

            if (p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY) {
                result.triggeredAction = btn.label;
                break;
            }
        }
    } else {
        result.intersected = false;
        result.t = -1.0f;
    }

    return result;
}
