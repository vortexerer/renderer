#pragma once
#include "../Math/Vector3.h"
#include "Colliders.h"

struct CollisionInfo {
    bool collided = false;
    Vector3 normal = Vector3(0.0f, 0.0f, 0.0f);
    float depth = 0.0f;
    Vector3 point = Vector3(0.0f, 0.0f, 0.0f);
};

class GJK_EPA {
public:
    static bool Intersection(const Collider* c1, const Collider* c2, CollisionInfo& info);
};
