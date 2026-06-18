#pragma once
#include <string>
#include <d3d12.h>
#include "../Math/Vector3.h"

class FontRenderer {
public:
    FontRenderer();
    ~FontRenderer();

    bool Initialize(ID3D12Device* device);
    void RenderText(const std::string& text, float x, float y, float scale, const Vector3& color);
    void Shutdown();
};
