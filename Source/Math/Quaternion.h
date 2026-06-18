#pragma once

#include "Matrix4x4.h"
#include <cmath>

struct Quaternion {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Quaternion() = default;
    Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}

    float LengthSquared() const {
        return w * w + x * x + y * y + z * z;
    }

    float Length() const {
        return std::sqrt(LengthSquared());
    }

    Quaternion Normalize() const {
        float len = Length();
        if (len < 1e-9f) {
            return Quaternion(0.0f, 0.0f, 0.0f, 0.0f);
        }
        return Quaternion(w / len, x / len, y / len, z / len);
    }

    Quaternion Conjugate() const {
        return Quaternion(w, -x, -y, -z);
    }

    // [고급수학Ⅰ 세특 탐구 연계 - 해밀턴 곱(식 4.32)]
    // 사원수의 곱셈(Hamilton Product)은 비가환 대수계(Non-commutative Algebra)로,
    // 허수 단위 관계 i^2 = j^2 = k^2 = ijk = -1 을 기반으로 전개됨.
    // 두 사원수의 회전을 합성하는 연산으로 벡터의 외적(Cross Product)과 내적(Dot Product)을 활용해 최적화 계산.
    Quaternion operator*(const Quaternion& other) const {
        return Quaternion(
            w * other.w - x * other.x - y * other.y - z * other.z,
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w
        );
    }

    Matrix4x4 ToRotationMatrix() const {
        Matrix4x4 res;
        res.m[0][0] = 1.0f - 2.0f * (y * y + z * z);
        res.m[0][1] = 2.0f * (x * y - w * z);
        res.m[0][2] = 2.0f * (x * z + w * y);
        res.m[0][3] = 0.0f;

        res.m[1][0] = 2.0f * (x * y + w * z);
        res.m[1][1] = 1.0f - 2.0f * (x * x + z * z);
        res.m[1][2] = 2.0f * (y * z - w * x);
        res.m[1][3] = 0.0f;

        res.m[2][0] = 2.0f * (x * z - w * y);
        res.m[2][1] = 2.0f * (y * z + w * x);
        res.m[2][2] = 1.0f - 2.0f * (x * x + y * y);
        res.m[2][3] = 0.0f;

        res.m[3][0] = 0.0f;
        res.m[3][1] = 0.0f;
        res.m[3][2] = 0.0f;
        res.m[3][3] = 1.0f;

        return res;
    }

    void ToEuler(float& heading, float& pitch, float& roll) const {
        float sinp = 2.0f * (y * z + w * x);
        if (sinp > 1.0f) sinp = 1.0f;
        if (sinp < -1.0f) sinp = -1.0f;

        pitch = std::asin(sinp);

        if (sinp > 0.99995f) {
            heading = 0.0f;
            roll = 2.0f * std::atan2(y, w);
        } else if (sinp < -0.99995f) {
            heading = 0.0f;
            roll = 2.0f * std::atan2(-y, w);
        } else {
            heading = std::atan2(2.0f * (w * y - x * z), 1.0f - 2.0f * (x * x + y * y));
            roll = std::atan2(2.0f * (w * z - x * y), 1.0f - 2.0f * (x * x + z * z));
        }
    }

    static Quaternion FromEuler(float heading, float pitch, float roll) {
        float h = heading * 0.5f;
        float p = pitch * 0.5f;
        float r = roll * 0.5f;

        float cos_h = std::cos(h), sin_h = std::sin(h);
        float cos_p = std::cos(p), sin_p = std::sin(p);
        float cos_r = std::cos(r), sin_r = std::sin(r);

        float w = cos_r * cos_p * cos_h - sin_r * sin_p * sin_h;
        float x = cos_r * sin_p * cos_h - sin_r * cos_p * sin_h;
        float y = cos_r * cos_p * sin_h + sin_r * sin_p * cos_h;
        float z = cos_r * sin_p * sin_h + sin_r * cos_p * cos_h;

        return Quaternion(w, x, y, z);
    }

    static Quaternion FromRotationMatrix(const Matrix4x4& mat) {
        float trace = mat.m[0][0] + mat.m[1][1] + mat.m[2][2];
        float w, x, y, z;
        if (trace > 0.0f) {
            float s = 2.0f * std::sqrt(trace + 1.0f);
            w = 0.25f * s;
            x = (mat.m[2][1] - mat.m[1][2]) / s;
            y = (mat.m[0][2] - mat.m[2][0]) / s;
            z = (mat.m[1][0] - mat.m[0][1]) / s;
        } else if ((mat.m[0][0] > mat.m[1][1]) && (mat.m[0][0] > mat.m[2][2])) {
            float s = 2.0f * std::sqrt(1.0f + mat.m[0][0] - mat.m[1][1] - mat.m[2][2]);
            w = (mat.m[2][1] - mat.m[1][2]) / s;
            x = 0.25f * s;
            y = (mat.m[0][1] + mat.m[1][0]) / s;
            z = (mat.m[0][2] + mat.m[2][0]) / s;
        } else if (mat.m[1][1] > mat.m[2][2]) {
            float s = 2.0f * std::sqrt(1.0f + mat.m[1][1] - mat.m[0][0] - mat.m[2][2]);
            w = (mat.m[0][2] - mat.m[2][0]) / s;
            x = (mat.m[0][1] + mat.m[1][0]) / s;
            y = 0.25f * s;
            z = (mat.m[1][2] + mat.m[2][1]) / s;
        } else {
            float s = 2.0f * std::sqrt(1.0f + mat.m[2][2] - mat.m[0][0] - mat.m[1][1]);
            w = (mat.m[1][0] - mat.m[0][1]) / s;
            x = (mat.m[0][2] + mat.m[2][0]) / s;
            y = (mat.m[1][2] + mat.m[2][1]) / s;
            z = 0.25f * s;
        }
        return Quaternion(w, x, y, z).Normalize();
    }

    // [고급수학Ⅰ 세특 탐구 연계 - 구면 선형 보간 SLERP(식 4.53)]
    // 두 사원수 q1과 q2 사이를 시간 변수 t(0~1)에 따라 4차원 초구면 상에서 등속 회전 경로로 보간.
    // 선형 보간(LERP)과 달리 회전 속도가 등속으로 유지되는 기하학적 장점이 있음.
    static Quaternion Slerp(const Quaternion& q1, Quaternion q2, float t) {
        float dot = q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z;

        if (dot < 0.0f) {
            q2 = Quaternion(-q2.w, -q2.x, -q2.y, -q2.z);
            dot = -dot;
        }

        if (dot > 0.9995f) {
            float w = q1.w + t * (q2.w - q1.w);
            float x = q1.x + t * (q2.x - q1.x);
            float y = q1.y + t * (q2.y - q1.y);
            float z = q1.z + t * (q2.z - q1.z);
            return Quaternion(w, x, y, z).Normalize();
        }

        float theta_0 = std::acos(dot);
        float theta = theta_0 * t;
        float sin_theta = std::sin(theta);
        float sin_theta_0 = std::sin(theta_0);

        // 공식: s0 = sin((1-t)*theta_0) / sin(theta_0) 
        // 삼각함수의 뺄셈정리를 통해 식을 전개하여 나눗셈 횟수를 최소화하여 아래와 같이 구현.
        float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
        float s1 = sin_theta / sin_theta_0;

        return Quaternion(
            s0 * q1.w + s1 * q2.w,
            s0 * q1.x + s1 * q2.x,
            s0 * q1.y + s1 * q2.y,
            s0 * q1.z + s1 * q2.z
        );
    }
};
