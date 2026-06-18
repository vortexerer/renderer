#include "AssetLoader.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <cctype>
#include <stdexcept>
#include "../Core/Logger.h"
#include "../Math/Matrix4x4.h"
#include "../Math/Quaternion.h"

struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<JsonValue> arrVal;
    std::unordered_map<std::string, JsonValue> objVal;

    bool Contains(const std::string& key) const {
        return type == Object && objVal.find(key) != objVal.end();
    }

    const JsonValue& Get(const std::string& key) const {
        auto it = objVal.find(key);
        if (it != objVal.end()) return it->second;
        static JsonValue nullVal;
        return nullVal;
    }
};

class JsonParser {
public:
    static JsonValue Parse(const std::string& src) {
        size_t offset = 0;
        return ParseValue(src, offset);
    }

private:
    static void SkipWhitespace(const std::string& src, size_t& offset) {
        while (offset < src.size() && std::isspace(static_cast<unsigned char>(src[offset]))) {
            offset++;
        }
    }

    static JsonValue ParseValue(const std::string& src, size_t& offset) {
        SkipWhitespace(src, offset);
        if (offset >= src.size()) return JsonValue{};

        char c = src[offset];
        if (c == '{') return ParseObject(src, offset);
        if (c == '[') return ParseArray(src, offset);
        if (c == '"') return ParseString(src, offset);
        if (src.compare(offset, 4, "true") == 0) { offset += 4; JsonValue v; v.type = JsonValue::Bool; v.boolVal = true; return v; }
        if (src.compare(offset, 5, "false") == 0) { offset += 5; JsonValue v; v.type = JsonValue::Bool; v.boolVal = false; return v; }
        if (src.compare(offset, 4, "null") == 0) { offset += 4; return JsonValue{}; }
        return ParseNumber(src, offset);
    }

    static JsonValue ParseString(const std::string& src, size_t& offset) {
        offset++; // skip open quote
        std::string s;
        while (offset < src.size() && src[offset] != '"') {
            if (src[offset] == '\\' && offset + 1 < src.size()) {
                offset++;
            }
            s += src[offset++];
        }
        if (offset < src.size()) offset++; // skip close quote
        JsonValue v;
        v.type = JsonValue::String;
        v.strVal = s;
        return v;
    }

    static JsonValue ParseNumber(const std::string& src, size_t& offset) {
        size_t start = offset;
        if (src[offset] == '-') offset++;
        while (offset < src.size() && (std::isdigit(static_cast<unsigned char>(src[offset])) || src[offset] == '.' || src[offset] == 'e' || src[offset] == 'E' || src[offset] == '+' || src[offset] == '-')) {
            offset++;
        }
        double num = std::stod(src.substr(start, offset - start));
        JsonValue v;
        v.type = JsonValue::Number;
        v.numVal = num;
        return v;
    }

    static JsonValue ParseArray(const std::string& src, size_t& offset) {
        offset++; // skip '['
        JsonValue v;
        v.type = JsonValue::Array;
        while (true) {
            SkipWhitespace(src, offset);
            if (offset < src.size() && src[offset] == ']') {
                offset++;
                break;
            }
            v.arrVal.push_back(ParseValue(src, offset));
            SkipWhitespace(src, offset);
            if (offset < src.size() && src[offset] == ',') {
                offset++;
            }
        }
        return v;
    }

    static JsonValue ParseObject(const std::string& src, size_t& offset) {
        offset++; // skip '{'
        JsonValue v;
        v.type = JsonValue::Object;
        while (true) {
            SkipWhitespace(src, offset);
            if (offset < src.size() && src[offset] == '}') {
                offset++;
                break;
            }
            JsonValue keyVal = ParseString(src, offset);
            SkipWhitespace(src, offset);
            if (offset < src.size() && src[offset] == ':') {
                offset++;
            }
            JsonValue val = ParseValue(src, offset);
            v.objVal[keyVal.strVal] = val;
            SkipWhitespace(src, offset);
            if (offset < src.size() && src[offset] == ',') {
                offset++;
            }
        }
        return v;
    }
};

struct GltfNode {
    std::string name;
    int meshIndex = -1;
    Vector3 translation = Vector3(0.0f, 0.0f, 0.0f);
    Quaternion rotation = Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);
    Matrix4x4 matrix = Matrix4x4::Identity();
    bool hasMatrix = false;
    std::vector<int> children;
};

struct GltfPrimitive {
    int positionAccessorIndex = -1;
};

struct GltfMesh {
    std::vector<GltfPrimitive> primitives;
};

struct GltfAccessor {
    Vector3 minVal = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 maxVal = Vector3(0.0f, 0.0f, 0.0f);
    bool hasBounds = false;
};

class GltfSceneTraverser {
public:
    std::vector<GltfNode> nodes;
    std::vector<GltfMesh> meshes;
    std::vector<GltfAccessor> accessors;

    Vector3 globalMin = Vector3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vector3 globalMax = Vector3(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    bool hasMeshBounds = false;
    std::vector<Vector3> nodeWorldPositions;

    void Traverse(int nodeIdx, const Matrix4x4& parentTransform) {
        const auto& node = nodes[nodeIdx];
        Matrix4x4 localTransform;
        if (node.hasMatrix) {
            localTransform = node.matrix;
        } else {
            Matrix4x4 T = Matrix4x4::Translation(node.translation.x, node.translation.y, node.translation.z);
            Matrix4x4 R = node.rotation.ToRotationMatrix();
            Matrix4x4 S = Matrix4x4::Scaling(node.scale.x, node.scale.y, node.scale.z);
            localTransform = T * R * S;
        }
        Matrix4x4 worldTransform = parentTransform * localTransform;

        // Record node position for node placement fallback
        nodeWorldPositions.push_back(worldTransform.TransformPoint(Vector3(0.0f, 0.0f, 0.0f)));

        if (node.meshIndex >= 0 && node.meshIndex < (int)meshes.size()) {
            const auto& mesh = meshes[node.meshIndex];
            for (const auto& prim : mesh.primitives) {
                if (prim.positionAccessorIndex >= 0 && prim.positionAccessorIndex < (int)accessors.size()) {
                    const auto& accessor = accessors[prim.positionAccessorIndex];
                    if (accessor.hasBounds) {
                        hasMeshBounds = true;
                        Vector3 localMin = accessor.minVal;
                        Vector3 localMax = accessor.maxVal;

                        Vector3 corners[8] = {
                            Vector3(localMin.x, localMin.y, localMin.z),
                            Vector3(localMin.x, localMin.y, localMax.z),
                            Vector3(localMin.x, localMax.y, localMin.z),
                            Vector3(localMin.x, localMax.y, localMax.z),
                            Vector3(localMax.x, localMin.y, localMin.z),
                            Vector3(localMax.x, localMin.y, localMax.z),
                            Vector3(localMax.x, localMax.y, localMin.z),
                            Vector3(localMax.x, localMax.y, localMax.z)
                        };

                        for (const auto& corner : corners) {
                            Vector3 worldCorner = worldTransform.TransformPoint(corner);
                            globalMin.x = std::min(globalMin.x, worldCorner.x);
                            globalMin.y = std::min(globalMin.y, worldCorner.y);
                            globalMin.z = std::min(globalMin.z, worldCorner.z);
                            globalMax.x = std::max(globalMax.x, worldCorner.x);
                            globalMax.y = std::max(globalMax.y, worldCorner.y);
                            globalMax.z = std::max(globalMax.z, worldCorner.z);
                        }
                    }
                }
            }
        }

        for (int childIdx : node.children) {
            Traverse(childIdx, worldTransform);
        }
    }
};

bool AssetLoader::LoadGLTF(const std::string& path, ModelData& outData) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::Error("Failed to open glTF file: " + path);
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    JsonValue root;
    try {
        root = JsonParser::Parse(content);
    } catch (const std::exception& e) {
        Logger::Error("JSON parse error: " + std::string(e.what()));
        return false;
    }

    GltfSceneTraverser traverser;

    // 1. Parse Accessors
    if (root.Contains("accessors")) {
        for (const auto& accVal : root.Get("accessors").arrVal) {
            GltfAccessor acc;
            if (accVal.Contains("min") && accVal.Contains("max")) {
                const auto& minA = accVal.Get("min").arrVal;
                const auto& maxA = accVal.Get("max").arrVal;
                if (minA.size() >= 3 && maxA.size() >= 3) {
                    acc.minVal = Vector3(static_cast<float>(minA[0].numVal), static_cast<float>(minA[1].numVal), static_cast<float>(minA[2].numVal));
                    acc.maxVal = Vector3(static_cast<float>(maxA[0].numVal), static_cast<float>(maxA[1].numVal), static_cast<float>(maxA[2].numVal));
                    acc.hasBounds = true;
                }
            }
            traverser.accessors.push_back(acc);
        }
    }

    // 2. Parse Meshes
    if (root.Contains("meshes")) {
        for (const auto& meshVal : root.Get("meshes").arrVal) {
            GltfMesh mesh;
            if (meshVal.Contains("primitives")) {
                for (const auto& primVal : meshVal.Get("primitives").arrVal) {
                    GltfPrimitive prim;
                    if (primVal.Contains("attributes")) {
                        const auto& attribs = primVal.Get("attributes");
                        if (attribs.Contains("POSITION")) {
                            prim.positionAccessorIndex = (int)attribs.Get("POSITION").numVal;
                        }
                    }
                    mesh.primitives.push_back(prim);
                }
            }
            traverser.meshes.push_back(mesh);
        }
    }

    // 3. Parse Nodes
    if (root.Contains("nodes")) {
        for (const auto& nodeVal : root.Get("nodes").arrVal) {
            GltfNode node;
            if (nodeVal.Contains("name")) node.name = nodeVal.Get("name").strVal;
            if (nodeVal.Contains("mesh")) node.meshIndex = (int)nodeVal.Get("mesh").numVal;
            
            if (nodeVal.Contains("translation")) {
                const auto& trans = nodeVal.Get("translation").arrVal;
                if (trans.size() >= 3) {
                    node.translation = Vector3(static_cast<float>(trans[0].numVal), static_cast<float>(trans[1].numVal), static_cast<float>(trans[2].numVal));
                }
            }
            if (nodeVal.Contains("rotation")) {
                const auto& rot = nodeVal.Get("rotation").arrVal;
                if (rot.size() >= 4) {
                    // glTF rotation is [x, y, z, w] -> Quaternion [w, x, y, z]
                    node.rotation = Quaternion(static_cast<float>(rot[3].numVal), static_cast<float>(rot[0].numVal), static_cast<float>(rot[1].numVal), static_cast<float>(rot[2].numVal));
                }
            }
            if (nodeVal.Contains("scale")) {
                const auto& sc = nodeVal.Get("scale").arrVal;
                if (sc.size() >= 3) {
                    node.scale = Vector3(static_cast<float>(sc[0].numVal), static_cast<float>(sc[1].numVal), static_cast<float>(sc[2].numVal));
                }
            }
            if (nodeVal.Contains("matrix")) {
                const auto& mat = nodeVal.Get("matrix").arrVal;
                if (mat.size() >= 16) {
                    node.hasMatrix = true;
                    for (int r = 0; r < 4; ++r) {
                        for (int c = 0; c < 4; ++c) {
                            node.matrix(r, c) = static_cast<float>(mat[r * 4 + c].numVal);
                        }
                    }
                }
            }
            if (nodeVal.Contains("children")) {
                for (const auto& childVal : nodeVal.Get("children").arrVal) {
                    node.children.push_back((int)childVal.numVal);
                }
            }
            traverser.nodes.push_back(node);
        }
    }

    if (traverser.nodes.empty()) {
        outData.minBounds = Vector3(0.0f, 0.0f, 0.0f);
        outData.maxBounds = Vector3(0.0f, 0.0f, 0.0f);
        return true;
    }

    // 4. Identify Root Nodes & Traverse
    std::vector<bool> isChild(traverser.nodes.size(), false);
    for (const auto& node : traverser.nodes) {
        for (int childIdx : node.children) {
            if (childIdx >= 0 && childIdx < (int)isChild.size()) {
                isChild[childIdx] = true;
            }
        }
    }

    for (size_t i = 0; i < traverser.nodes.size(); ++i) {
        if (!isChild[i]) {
            traverser.Traverse(static_cast<int>(i), Matrix4x4::Identity());
        }
    }

    // 5. Populate Output Bounds
    if (traverser.hasMeshBounds) {
        outData.minBounds = traverser.globalMin;
        outData.maxBounds = traverser.globalMax;
    } else {
        // Fallback: Compute bounding box based on scene node placements
        Vector3 nodeMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
        Vector3 nodeMax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
        for (const auto& pos : traverser.nodeWorldPositions) {
            nodeMin.x = std::min(nodeMin.x, pos.x);
            nodeMin.y = std::min(nodeMin.y, pos.y);
            nodeMin.z = std::min(nodeMin.z, pos.z);
            nodeMax.x = std::max(nodeMax.x, pos.x);
            nodeMax.y = std::max(nodeMax.y, pos.y);
            nodeMax.z = std::max(nodeMax.z, pos.z);
        }
        outData.minBounds = nodeMin;
        outData.maxBounds = nodeMax;
    }

    // 6. Populate Output Node Metadata
    for (const auto& node : traverser.nodes) {
        AssetLoader::Node outNode;
        outNode.name = node.name;
        outNode.position = node.translation;
        outNode.isStatic = (node.name.rfind("Static_", 0) == 0);
        outData.nodes.push_back(outNode);
    }

    return true;
}
