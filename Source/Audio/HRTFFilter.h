#pragma once
#include "../Math/Vector3.h"

struct SpatialParameters {
    float distance;
    float azimuthDegrees;
    float dopplerFactor;
    float itdDelaySec;
    float ildLeft;
    float ildRight;
};

class HRTFFilter {
public:
    static SpatialParameters CalculateSpatialParams(
        const Vector3& srcPos, const Vector3& srcVel,
        const Vector3& listPos, const Vector3& listVel
    );
};
