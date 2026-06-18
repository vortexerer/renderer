#include "Colliders.h"
#include <algorithm>

// SphereCollider
SphereCollider::SphereCollider(const Vector3& center, float radius) : center(center), radius(radius) {}

Vector3 SphereCollider::GetSupport(const Vector3& direction) const {
    Vector3 normDir = direction.Normalize();
    return center + normDir * radius;
}

// OBBCollider
OBBCollider::OBBCollider(const Vector3& center, const Vector3& extents, const Quaternion& orientation)
    : center(center), extents(extents), orientation(orientation) {}

Vector3 OBBCollider::GetSupport(const Vector3& direction) const {
    // Transform direction to local space (using conjugate rotation)
    Quaternion conj = orientation.Conjugate();
    Matrix4x4 toLocal = conj.ToRotationMatrix();
    Vector3 localDir = toLocal.TransformVector(direction);

    // Get support point in local OBB space
    Vector3 localSupport(
        (localDir.x >= 0.0f ? extents.x : -extents.x),
        (localDir.y >= 0.0f ? extents.y : -extents.y),
        (localDir.z >= 0.0f ? extents.z : -extents.z)
    );

    // Transform local support point back to world space
    Matrix4x4 toWorld = orientation.ToRotationMatrix();
    return center + toWorld.TransformVector(localSupport);
}

// ConvexHullCollider
ConvexHullCollider::ConvexHullCollider(const std::vector<Vector3>& vertices) : vertices(vertices) {}

Vector3 ConvexHullCollider::GetSupport(const Vector3& direction) const {
    if (vertices.empty()) return Vector3(0.0f, 0.0f, 0.0f);
    
    float maxDot = -1e30f;
    Vector3 supportPoint = vertices[0];
    
    for (const auto& v : vertices) {
        float dot = v.Dot(direction);
        if (dot > maxDot) {
            maxDot = dot;
            supportPoint = v;
        }
    }
    return supportPoint;
}
