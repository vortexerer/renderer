#pragma once
#include "../Math/Vector3.h"

class Collider;

class Rigidbody {
public:
    Rigidbody();
    ~Rigidbody();

    Vector3 position;
    Vector3 velocity;
    Vector3 acceleration;
    Vector3 force;
    float mass;
    float restitution;
    bool isStatic;
    bool useGravity;
    Collider* collider;

    void ApplyForce(const Vector3& f);
    void ClearForces();
};
