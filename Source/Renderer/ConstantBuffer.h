#pragma once
#include "../Math/Matrix4x4.h"

struct ConstantBuffer {
    Matrix4x4 View[2];
    Matrix4x4 Projection[2];
    Matrix4x4 Model;
    float padding[16]; // 64 bytes padding, making total 384 bytes, let's pad to 512 bytes
    float padding2[32]; // total size: 128 + 128 + 64 + 64 + 128 = 512 bytes
};
