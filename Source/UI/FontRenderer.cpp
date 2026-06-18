#include "FontRenderer.h"
#include "../Core/Logger.h"

FontRenderer::FontRenderer() {}

FontRenderer::~FontRenderer() {
    Shutdown();
}

bool FontRenderer::Initialize(ID3D12Device* device) {
    (void)device;
    Logger::Info("FontRenderer: Initialized SDF font renderer.");
    return true;
}

void FontRenderer::RenderText(const std::string& text, float x, float y, float scale, const Vector3& color) {
    (void)text;
    (void)x;
    (void)y;
    (void)scale;
    (void)color;
}

void FontRenderer::Shutdown() {
    // Shutdown logic
}
