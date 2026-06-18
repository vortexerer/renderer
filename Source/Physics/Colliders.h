#pragma once
#include <vector>
#include "../Math/Vector3.h"
#include "../Math/Quaternion.h"

enum class ColliderType {
    Sphere,
    OBB,
    ConvexHull
};

class Collider {
public:
    virtual ~Collider() {}
    virtual ColliderType GetType() const = 0;
    virtual Vector3 GetSupport(const Vector3& direction) const = 0;
};

class SphereCollider : public Collider {
public:
    SphereCollider(const Vector3& center, float radius);
    virtual ColliderType GetType() const override { return ColliderType::Sphere; }
    virtual Vector3 GetSupport(const Vector3& direction) const override;

    Vector3 center;
    float radius;
};

class OBBCollider : public Collider {
public:
    OBBCollider(const Vector3& center, const Vector3& extents, const Quaternion& orientation);
    virtual ColliderType GetType() const override { return ColliderType::OBB; }
    virtual Vector3 GetSupport(const Vector3& direction) const override;

    Vector3 center;
    Vector3 extents;
    Quaternion orientation;
};

class ConvexHullCollider : public Collider {
public:
    ConvexHullCollider(const std::vector<Vector3>& vertices);
    virtual ColliderType GetType() const override { return ColliderType::ConvexHull; }
    virtual Vector3 GetSupport(const Vector3& direction) const override;

    std::vector<Vector3> vertices;
};
