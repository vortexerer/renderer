#pragma once
#include <string>
#include <vector>
#include "../Math/Vector3.h"

class AssetLoader {
public:
    struct Node {
        std::string name;
        Vector3 position;
        bool isStatic;
    };

    struct ModelData {
        std::vector<Node> nodes;
        Vector3 minBounds;
        Vector3 maxBounds;
    };

    static bool LoadGLTF(const std::string& path, ModelData& outData);
};
