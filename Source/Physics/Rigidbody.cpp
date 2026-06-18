#include "Rigidbody.h"

Rigidbody::Rigidbody() :
    position(0.0f, 0.0f, 0.0f),
    velocity(0.0f, 0.0f, 0.0f),
    acceleration(0.0f, 0.0f, 0.0f),
    force(0.0f, 0.0f, 0.0f),
    mass(1.0f),
    restitution(0.8f),
    isStatic(false),
    useGravity(true),
    collider(nullptr) {}

Rigidbody::~Rigidbody() {}

void Rigidbody::ApplyForce(const Vector3& f) {
    if (!isStatic) {
        force += f;
    }
}

void Rigidbody::ClearForces() {
    force = Vector3(0.0f, 0.0f, 0.0f);
}
