#include "HRTFFilter.h"
#include <cmath>
#include <algorithm>

SpatialParameters HRTFFilter::CalculateSpatialParams(
    const Vector3& srcPos, const Vector3& srcVel,
    const Vector3& listPos, const Vector3& listVel
) {
    float c = 343.0f;
    Vector3 d = listPos - srcPos;
    float dist = d.Length();
    Vector3 u = dist > 0.0f ? d / dist : Vector3(0.0f, 0.0f, -1.0f);

    float v_l = listVel.Dot(u);
    float v_s = srcVel.Dot(u);

    if (v_s >= c) v_s = c - 0.01f;
    if (v_l >= c) v_l = c - 0.01f;

    float doppler_factor = (c - v_l) / (c - v_s);

    float dx = d.x;
    float dz = d.z;
    float norm_xz = std::sqrt(dx * dx + dz * dz);
    float azimuth = 0.0f;
    if (norm_xz > 0.0f) {
        dx /= norm_xz;
        dz /= norm_xz;
        azimuth = std::atan2(-dx, -dz);
    }

    float dist_atten = dist > 0.0f ? 1.0f / (dist * dist + 1.0f) : 1.0f;
    float dw = 0.175f;
    float itd_delay = (dw / (2.0f * c)) * std::sin(azimuth);

    float ild_left = dist_atten * (1.0f - 0.5f * std::max(0.0f, std::sin(azimuth)));
    float ild_right = dist_atten * (1.0f - 0.5f * std::max(0.0f, -std::sin(azimuth)));

    float azimuth_degrees = azimuth * 180.0f / 3.141592653589793f;

    SpatialParameters params;
    params.distance = dist;
    params.azimuthDegrees = azimuth_degrees;
    params.dopplerFactor = doppler_factor;
    params.itdDelaySec = itd_delay;
    params.ildLeft = ild_left;
    params.ildRight = ild_right;
    return params;
}
