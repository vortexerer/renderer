#include "PhysicsEngine.h"
#include "GJK_EPA.h"
#include "Colliders.h"
#include "../Math/Quaternion.h"
#include <algorithm>
#include <cmath>

PhysicsEngine::PhysicsEngine() {}

PhysicsEngine::~PhysicsEngine() {
    m_Bodies.clear();
}

// [고급물리학 세특 탐구 연계 - Fixed Timestep Accumulator]
// 렌더 프레임율의 급격한 편차로 인한 물리 오차 누적과 터널링 현상을 막기 위해
// 가변 델타 타임(frameTime)을 누적기(Accumulator)에 쌓아두고,
// 항상 고정된 시간 격자(m_FixedTimeStep = 0.01초 = 100Hz) 단위로만 물리 시뮬레이션을 업데이트함.
void PhysicsEngine::Update(double frameTime) {
    m_Accumulator += frameTime;
    while (m_Accumulator >= m_FixedTimeStep) {
        FixedUpdate(m_FixedTimeStep);
        m_Accumulator -= m_FixedTimeStep;
    }
}

void PhysicsEngine::AddRigidbody(Rigidbody* rb) {
    if (rb && std::find(m_Bodies.begin(), m_Bodies.end(), rb) == m_Bodies.end()) {
        m_Bodies.push_back(rb);
    }
}

void PhysicsEngine::RemoveRigidbody(Rigidbody* rb) {
    auto it = std::find(m_Bodies.begin(), m_Bodies.end(), rb);
    if (it != m_Bodies.end()) {
        m_Bodies.erase(it);
    }
}

void PhysicsEngine::FixedUpdate(double dt) {
    IntegrateForces(dt);

    // Snapshot positions before the position integration so the continuous
    // collision sweep knows where each body started this step.
    std::vector<Vector3> prevPositions(m_Bodies.size());
    for (size_t i = 0; i < m_Bodies.size(); ++i) {
        prevPositions[i] = m_Bodies[i]->position;
    }

    IntegrateVelocities(dt);

    // [고급물리학 세특 탐구 연계 - 연속 충돌 검출(CCD)]
    // 한 스텝 내 빠른 이동으로 인한 터널링/깊은 관통을 막기 위해 이동 구간을 스윕하여
    // 가장 이른 충돌 시점(Time of Impact)에서 위치를 접촉면으로 고정하고 반사 충격량을 적용함.
    if (m_EnableCCD) {
        ApplyContinuousCollision(prevPositions);
    }

    // Discrete impulse + Baumgarte stabilization handles resting/residual contacts.
    ResolveCollisions(dt);

    // Clear forces
    for (auto* body : m_Bodies) {
        body->ClearForces();
    }
}

// [고급물리학 세특 탐구 연계 - 반암시적 오일러(Semi-implicit Euler) 수치 적분]
// 일반적인 오일러 기법(Explicit Euler)은 매 스텝마다 에너지가 증가하여 발산(폭주)하는 한계가 있음.
// 반면, 본 엔진에 적용한 반암시적 오일러(Symplectic Euler) 방식은 새로 계산된 속도 v_(n+1)를 구한 뒤,
// 이를 위치 r_(n+1) 계산의 입력으로 사용하여 에너지가 일정 한계 내에서 보존되도록 안정성을 실증한 수치 해석 기법임.
void PhysicsEngine::IntegrateForces(double dt) {
    for (auto* body : m_Bodies) {
        if (body->isStatic) continue;

        Vector3 gravityForce(0.0f, 0.0f, 0.0f);
        if (body->useGravity) {
            // 뉴턴의 제2법칙 F = m * a 로부터 유도
            gravityForce = Vector3(0.0f, -9.81f * body->mass, 0.0f);
        }
        Vector3 totalForce = body->force + gravityForce;

        // 가속도 계산: a = F / m
        body->acceleration = totalForce / body->mass;
        // 속도 적분: v_(n+1) = v_n + a_n * dt (속도를 먼저 갱신)
        body->velocity += body->acceleration * static_cast<float>(dt);
    }
}

void PhysicsEngine::IntegrateVelocities(double dt) {
    for (auto* body : m_Bodies) {
        if (body->isStatic) continue;
        // 위치 적분: r_(n+1) = r_n + v_(n+1) * dt (갱신된 속도 v_(n+1)을 위치 계산에 대입)
        body->position += body->velocity * static_cast<float>(dt);
    }
}

// ---------------------------------------------------------------------------
// Geometry helpers (file-local)
// ---------------------------------------------------------------------------
namespace {

// Rotate a vector by a quaternion: v' = q * v * q^-1, expanded for efficiency.
Vector3 RotateByQuat(const Quaternion& q, const Vector3& v) {
    Vector3 u(q.x, q.y, q.z);
    float s = q.w;
    return u * (2.0f * u.Dot(v)) + v * (s * s - u.Dot(u)) + u.Cross(v) * (2.0f * s);
}

void SyncColliderCenter(Rigidbody* body) {
    if (!body || !body->collider) return;
    if (body->collider->GetType() == ColliderType::Sphere) {
        static_cast<SphereCollider*>(body->collider)->center = body->position;
    } else if (body->collider->GetType() == ColliderType::OBB) {
        static_cast<OBBCollider*>(body->collider)->center = body->position;
    }
}

// Earliest time-of-impact in [0,1] of a sphere whose center sweeps P0->P1
// (radius r) against a stationary sphere (center C, radius R). Returns false
// if no contact occurs within the segment.
bool SweepSphereVsSphere(const Vector3& P0, const Vector3& P1, float r,
                         const Vector3& C, float R, float& tHit) {
    Vector3 d = P1 - P0;        // motion of the moving sphere center
    Vector3 m = P0 - C;         // start offset from target center
    float radii = r + R;

    // Already overlapping at the start of the step -> let the discrete solver handle it.
    if (m.LengthSquared() <= radii * radii) {
        return false;
    }

    float a = d.Dot(d);
    if (a < 1e-12f) return false; // no motion
    float b = 2.0f * m.Dot(d);
    float c = m.Dot(m) - radii * radii;

    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false; // never within contact distance
    float sq = std::sqrt(disc);
    float t = (-b - sq) / (2.0f * a); // earliest root
    if (t < 0.0f || t > 1.0f) return false;
    tHit = t;
    return true;
}

// Earliest time-of-impact in [0,1] of a sphere (radius r) sweeping P0->P1
// against a (possibly oriented) box. The box is treated in its local frame as
// an AABB expanded by the sphere radius (Minkowski sum), and the segment is
// intersected with that slab. Also returns the outward contact normal (world).
bool SweepSphereVsOBB(const Vector3& P0, const Vector3& P1, float r,
                      const OBBCollider* box, float& tHit, Vector3& normalOut) {
    Quaternion inv = box->orientation.Conjugate();
    Vector3 l0 = RotateByQuat(inv, P0 - box->center);
    Vector3 l1 = RotateByQuat(inv, P1 - box->center);
    Vector3 d = l1 - l0;

    Vector3 ext = box->extents + Vector3(r, r, r); // expanded half-extents

    // Already inside the expanded box at the start -> defer to discrete solver.
    if (std::abs(l0.x) <= ext.x && std::abs(l0.y) <= ext.y && std::abs(l0.z) <= ext.z) {
        return false;
    }

    float tmin = 0.0f, tmax = 1.0f;
    int hitAxis = -1;
    float hitSign = 0.0f;
    const float lo[3] = { -ext.x, -ext.y, -ext.z };
    const float hi[3] = {  ext.x,  ext.y,  ext.z };
    const float o[3]  = { l0.x, l0.y, l0.z };
    const float dir[3]= { d.x,  d.y,  d.z  };

    for (int i = 0; i < 3; ++i) {
        if (std::abs(dir[i]) < 1e-12f) {
            if (o[i] < lo[i] || o[i] > hi[i]) return false; // parallel & outside slab
        } else {
            float inv_d = 1.0f / dir[i];
            float t1 = (lo[i] - o[i]) * inv_d;
            float t2 = (hi[i] - o[i]) * inv_d;
            float sign = -1.0f;
            if (t1 > t2) { std::swap(t1, t2); sign = 1.0f; }
            if (t1 > tmin) { tmin = t1; hitAxis = i; hitSign = sign; }
            if (t2 < tmax) { tmax = t2; }
            if (tmin > tmax) return false;
        }
    }

    if (hitAxis < 0 || tmin < 0.0f || tmin > 1.0f) return false;

    // Outward face normal in box-local space, rotated back to world space.
    Vector3 localN(0.0f, 0.0f, 0.0f);
    if (hitAxis == 0) localN.x = hitSign;
    else if (hitAxis == 1) localN.y = hitSign;
    else localN.z = hitSign;
    normalOut = RotateByQuat(box->orientation, localN);

    tHit = tmin;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Continuous Collision Detection
// ---------------------------------------------------------------------------
void PhysicsEngine::ApplyContinuousCollision(const std::vector<Vector3>& prevPositions) {
    for (size_t i = 0; i < m_Bodies.size(); ++i) {
        Rigidbody* A = m_Bodies[i];
        if (A->isStatic || !A->collider) continue;
        if (A->collider->GetType() != ColliderType::Sphere) continue; // CCD limited to spheres

        float rA = static_cast<SphereCollider*>(A->collider)->radius;
        Vector3 P0 = prevPositions[i];
        Vector3 P1 = A->position;

        float bestT = 2.0f;
        Vector3 bestNormal(0.0f, 1.0f, 0.0f);
        Rigidbody* bestB = nullptr;

        for (size_t j = 0; j < m_Bodies.size(); ++j) {
            if (j == i) continue;
            Rigidbody* B = m_Bodies[j];
            if (!B->collider) continue;

            // Treat B as stationary at its current position for the sweep.
            SyncColliderCenter(B);

            float t = 2.0f;
            Vector3 n(0.0f, 1.0f, 0.0f);
            bool hit = false;

            if (B->collider->GetType() == ColliderType::Sphere) {
                const SphereCollider* sb = static_cast<const SphereCollider*>(B->collider);
                if (SweepSphereVsSphere(P0, P1, rA, sb->center, sb->radius, t)) {
                    Vector3 contact = P0 + (P1 - P0) * t;
                    Vector3 delta = contact - sb->center;
                    n = delta.LengthSquared() > 1e-12f ? delta.Normalize() : Vector3(0.0f, 1.0f, 0.0f);
                    hit = true;
                }
            } else if (B->collider->GetType() == ColliderType::OBB) {
                const OBBCollider* ob = static_cast<const OBBCollider*>(B->collider);
                if (SweepSphereVsOBB(P0, P1, rA, ob, t, n)) {
                    hit = true;
                }
            }

            if (hit && t < bestT) {
                bestT = t;
                bestNormal = n;
                bestB = B;
            }
        }

        if (bestB && bestT <= 1.0f) {
            // Clamp the body to the time-of-impact contact position (no penetration).
            Vector3 contact = P0 + (P1 - P0) * bestT;
            A->position = contact;
            SyncColliderCenter(A);

            // Reflect velocity using the standard restitution impulse so the
            // bounce happens exactly at the contact surface.
            Vector3 n = bestNormal;
            Vector3 rv = A->velocity - bestB->velocity;
            float velAlongNormal = rv.Dot(n);
            if (velAlongNormal < 0.0f) {
                float e = std::min(A->restitution, bestB->restitution);
                float invA = A->isStatic ? 0.0f : 1.0f / A->mass;
                float invB = bestB->isStatic ? 0.0f : 1.0f / bestB->mass;
                float invSum = invA + invB;
                if (invSum > 0.0f) {
                    float jImp = -(1.0f + e) * velAlongNormal / invSum;
                    A->velocity += n * (jImp * invA);
                    if (!bestB->isStatic) bestB->velocity -= n * (jImp * invB);
                }
            }
        }
    }
}

void PhysicsEngine::ResolveCollisions(double dt) {
    for (size_t i = 0; i < m_Bodies.size(); ++i) {
        for (size_t j = i + 1; j < m_Bodies.size(); ++j) {
            Rigidbody* bA = m_Bodies[i];
            Rigidbody* bB = m_Bodies[j];

            if (bA->isStatic && bB->isStatic) continue;
            if (!bA->collider || !bB->collider) continue;

            // Sync collider position with body position
            SyncColliderCenter(bA);
            SyncColliderCenter(bB);

            CollisionInfo info;
            if (GJK_EPA::Intersection(bA->collider, bB->collider, info)) {
                Vector3 normal = info.normal; // Points from B to A

                // Relative velocity
                Vector3 rv = bA->velocity - bB->velocity;
                float velAlongNormal = rv.Dot(normal);

                float e = std::min(bA->restitution, bB->restitution);
                float massA_inv = bA->isStatic ? 0.0f : 1.0f / bA->mass;
                float massB_inv = bB->isStatic ? 0.0f : 1.0f / bB->mass;
                float total_mass_inv = massA_inv + massB_inv;
                if (total_mass_inv <= 0.0f) continue;

                // [고급물리학 세특 탐구 연계 - Baumgarte 안정화]
                // 이산 적분으로 남은 관통을 한 번에 순간이동(teleport)시키면 에너지가 튀어
                // 불안정해지므로, 관통량에 비례한 보정 속도 bias = (beta/dt)*max(d - slop, 0)를
                // 충격량에 더해 여러 스텝에 걸쳐 부드럽고 안정적으로 관통을 해소함.
                float penetration = std::max(info.depth - m_PenetrationSlop, 0.0f);
                float bias = (m_Baumgarte / static_cast<float>(dt)) * penetration;

                // Apply an impulse if bodies approach OR there is penetration to push out.
                if (velAlongNormal < 0.0f || bias > 0.0f) {
                    float restitutionTerm = (velAlongNormal < 0.0f) ? -(1.0f + e) * velAlongNormal : 0.0f;
                    float jImp = (restitutionTerm + bias) / total_mass_inv;

                    if (!bA->isStatic) bA->velocity += normal * (jImp * massA_inv);
                    if (!bB->isStatic) bB->velocity -= normal * (jImp * massB_inv);
                }
            }
        }
    }
}
