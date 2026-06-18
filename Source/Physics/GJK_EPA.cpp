#include "GJK_EPA.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Helper to calculate support point of Minkowski difference
static Vector3 Support(const Collider* c1, const Collider* c2, const Vector3& direction) {
    return c1->GetSupport(direction) - c2->GetSupport(-direction);
}

// Next simplex handler for GJK
static bool NextSimplex(std::vector<Vector3>& simplex, Vector3& direction) {
    if (simplex.size() == 2) {
        // Line segment
        Vector3 B = simplex[0];
        Vector3 A = simplex[1];
        Vector3 AB = B - A;
        Vector3 AO = -A;
        if (AB.Dot(AO) > 0.0f) {
            direction = AB.Cross(AO).Cross(AB);
            if (direction.LengthSquared() < 1e-9f) {
                // AB and AO are collinear
                direction = Vector3(-AB.y, AB.x, 0.0f);
                if (direction.LengthSquared() < 1e-9f) {
                    direction = Vector3(0.0f, -AB.z, AB.y);
                }
            }
            direction = direction.Normalize();
        } else {
            simplex = { A };
            direction = AO.Normalize();
        }
    } else if (simplex.size() == 3) {
        // Triangle
        Vector3 C = simplex[0];
        Vector3 B = simplex[1];
        Vector3 A = simplex[2];
        Vector3 AB = B - A;
        Vector3 AC = C - A;
        Vector3 ABC = AB.Cross(AC);
        Vector3 AO = -A;

        if (ABC.Cross(AC).Dot(AO) > 0.0f) {
            if (AC.Dot(AO) > 0.0f) {
                simplex = { C, A };
                direction = AC.Cross(AO).Cross(AC).Normalize();
            } else {
                if (AB.Dot(AO) > 0.0f) {
                    simplex = { B, A };
                    direction = AB.Cross(AO).Cross(AB).Normalize();
                } else {
                    simplex = { A };
                    direction = AO.Normalize();
                }
            }
        } else {
            if (AB.Cross(ABC).Dot(AO) > 0.0f) {
                if (AB.Dot(AO) > 0.0f) {
                    simplex = { B, A };
                    direction = AB.Cross(AO).Cross(AB).Normalize();
                } else {
                    simplex = { A };
                    direction = AO.Normalize();
                }
            } else {
                if (ABC.Dot(AO) > 0.0f) {
                    direction = ABC.Normalize();
                } else {
                    simplex = { B, C, A };
                    direction = -ABC.Normalize();
                }
            }
        }
    } else if (simplex.size() == 4) {
        // Tetrahedron
        Vector3 D = simplex[0];
        Vector3 C = simplex[1];
        Vector3 B = simplex[2];
        Vector3 A = simplex[3];

        Vector3 AB = B - A;
        Vector3 AC = C - A;
        Vector3 AD = D - A;
        Vector3 AO = -A;

        Vector3 ABC = AB.Cross(AC);
        Vector3 ACD = AC.Cross(AD);
        Vector3 ADB = AD.Cross(AB);

        // Check face ABC
        if (ABC.Dot(AO) > 0.0f) {
            simplex = { C, B, A };
            direction = ABC.Normalize();
            return false;
        }
        // Check face ACD
        if (ACD.Dot(AO) > 0.0f) {
            simplex = { D, C, A };
            direction = ACD.Normalize();
            return false;
        }
        // Check face ADB
        if (ADB.Dot(AO) > 0.0f) {
            simplex = { B, D, A };
            direction = ADB.Normalize();
            return false;
        }
        return true; // Origin is inside all faces, collision!
    }
    return false;
}

// Struct to represent a triangle face in EPA polytope
struct Face {
    Vector3 a, b, c;
    Vector3 normal;
    float distance;
};

// Compute normal and distance from origin for a triangle face
static Face MakeFace(const Vector3& a, const Vector3& b, const Vector3& c) {
    Face face;
    face.a = a;
    face.b = b;
    face.c = c;
    Vector3 normal = (b - a).Cross(c - a).Normalize();
    face.normal = normal;
    face.distance = normal.Dot(a);
    if (face.distance < 0.0f) {
        face.normal = -normal;
        face.distance = -face.distance;
    }
    return face;
}

// EPA Implementation
static void EPA(const std::vector<Vector3>& simplex, const Collider* c1, const Collider* c2, CollisionInfo& info) {
    if (simplex.size() < 4) return;

    std::vector<Face> faces;
    faces.push_back(MakeFace(simplex[0], simplex[1], simplex[2]));
    faces.push_back(MakeFace(simplex[0], simplex[2], simplex[3]));
    faces.push_back(MakeFace(simplex[0], simplex[3], simplex[1]));
    faces.push_back(MakeFace(simplex[1], simplex[3], simplex[2]));

    float min_dist = 1e30f;
    Face closest_face;

    const int MAX_ITERATIONS = 30;
    for (int iter = 0; iter < MAX_ITERATIONS; ++iter) {
        min_dist = 1e30f;
        size_t closest_idx = 0;
        
        for (size_t i = 0; i < faces.size(); ++i) {
            if (faces[i].distance < min_dist) {
                min_dist = faces[i].distance;
                closest_face = faces[i];
                closest_idx = i;
            }
        }

        Vector3 p = Support(c1, c2, closest_face.normal);
        float d = p.Dot(closest_face.normal);

        if (d - min_dist < 0.001f) {
            // Found penetration depth and normal!
            info.depth = min_dist;
            info.normal = closest_face.normal;
            
            // Calculate contact point (on surface of first collider)
            info.point = closest_face.a * 0.3333f + closest_face.b * 0.3333f + closest_face.c * 0.3333f;
            return;
        }

        // Split the face and expand polytope
        // For simplicity, we remove the closest face and insert three new faces
        Vector3 a = closest_face.a;
        Vector3 b = closest_face.b;
        Vector3 c = closest_face.c;

        faces.erase(faces.begin() + closest_idx);
        faces.push_back(MakeFace(a, b, p));
        faces.push_back(MakeFace(b, c, p));
        faces.push_back(MakeFace(c, a, p));
    }

    info.depth = min_dist;
    info.normal = closest_face.normal;
    info.point = closest_face.a * 0.3333f + closest_face.b * 0.3333f + closest_face.c * 0.3333f;
}

bool GJK_EPA::Intersection(const Collider* c1, const Collider* c2, CollisionInfo& info) {
    if (!c1 || !c2) return false;

    // Special case for Sphere vs Sphere to ensure maximum reliability and speed
    if (c1->GetType() == ColliderType::Sphere && c2->GetType() == ColliderType::Sphere) {
        const SphereCollider* s1 = static_cast<const SphereCollider*>(c1);
        const SphereCollider* s2 = static_cast<const SphereCollider*>(c2);
        Vector3 d = s1->center - s2->center;
        float dist = d.Length();
        float rSum = s1->radius + s2->radius;
        if (dist <= rSum) {
            info.collided = true;
            info.normal = dist > 0.0f ? d.Normalize() : Vector3(0.0f, 1.0f, 0.0f);
            info.depth = rSum - dist;
            info.point = s2->center + info.normal * s2->radius;
            return true;
        }
        return false;
    }

    std::vector<Vector3> simplex;
    Vector3 dir(1.0f, 0.0f, 0.0f);
    simplex.push_back(Support(c1, c2, dir));
    dir = -simplex[0];
    if (dir.LengthSquared() < 1e-9f) {
        dir = Vector3(0.0f, 1.0f, 0.0f);
    }
    dir = dir.Normalize();

    const int GJK_MAX_ITER = 30;
    bool collided = false;
    
    for (int iter = 0; iter < GJK_MAX_ITER; ++iter) {
        Vector3 p = Support(c1, c2, dir);
        if (p.Dot(dir) < 0.0f) {
            collided = false;
            break;
        }
        simplex.push_back(p);
        if (NextSimplex(simplex, dir)) {
            collided = true;
            break;
        }
    }

    info.collided = collided;
    if (collided) {
        // Expand to find contact details
        EPA(simplex, c1, c2, info);
    }
    return collided;
}
