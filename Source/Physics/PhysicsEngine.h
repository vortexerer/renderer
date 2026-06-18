#pragma once
#include <vector>
#include "Rigidbody.h"

class PhysicsEngine {
public:
    PhysicsEngine();
    ~PhysicsEngine();

    void Update(double frameTime);
    void AddRigidbody(Rigidbody* rb);
    void RemoveRigidbody(Rigidbody* rb);

    void FixedUpdate(double dt);
    const std::vector<Rigidbody*>& GetBodies() const { return m_Bodies; }

    // Continuous Collision Detection toggle (enabled by default). Exposed so the
    // demo/tools can disable it for A/B comparisons of tunnelling behaviour.
    void SetContinuousCollision(bool enabled) { m_EnableCCD = enabled; }
    bool IsContinuousCollisionEnabled() const { return m_EnableCCD; }

private:
    void IntegrateForces(double dt);
    void IntegrateVelocities(double dt);

    // Continuous Collision Detection: sweep dynamic sphere bodies from their
    // pre-integration position to their new position and resolve at the earliest
    // time-of-impact, preventing tunnelling and deep single-step penetration.
    void ApplyContinuousCollision(const std::vector<Vector3>& prevPositions);

    // Discrete impulse solver with Baumgarte positional stabilization for
    // resting/residual contacts that CCD does not fully consume.
    void ResolveCollisions(double dt);

    std::vector<Rigidbody*> m_Bodies;
    double m_Accumulator = 0.0;
    const double m_FixedTimeStep = 0.01; // 100Hz

    // --- Solver tuning -------------------------------------------------------
    bool m_EnableCCD = true;
    // Baumgarte stabilization: fraction of penetration corrected per step via a
    // velocity bias (beta/dt * penetration). Small values keep the solver stable.
    const float m_Baumgarte = 0.2f;
    // Penetration allowance ("slop") left uncorrected to avoid jitter at rest.
    const float m_PenetrationSlop = 0.005f;
};
