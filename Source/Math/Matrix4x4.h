#pragma once

#include "Vector3.h"

struct Matrix4x4 {
    float m[4][4] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f}
    };

    Matrix4x4() = default;

    float operator()(int row, int col) const {
        return m[row][col];
    }
    float& operator()(int row, int col) {
        return m[row][col];
    }

    Matrix4x4 operator*(const Matrix4x4& other) const {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i][j] = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    result.m[i][j] += m[i][k] * other.m[k][j];
                }
            }
        }
        return result;
    }

    Vector3 Transform(const Vector3& v, float w) const {
        float x = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * w;
        float y = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * w;
        float z = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * w;
        float out_w = m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * w;
        if (std::abs(out_w) > 1e-9f && w != 0.0f) {
            return Vector3(x / out_w, y / out_w, z / out_w);
        }
        return Vector3(x, y, z);
    }

    Vector3 TransformPoint(const Vector3& v) const {
        return Transform(v, 1.0f);
    }

    Vector3 TransformVector(const Vector3& v) const {
        return Transform(v, 0.0f);
    }

    // [고급수학Ⅰ 세특 탐구 연계 - 법선 변환]
    // 비균등 스케일링(Non-uniform scaling)이 가해진 물체의 법선 벡터(Normal Vector)는
    // 일반 아핀 변환 행렬 M을 곱하면 수직 기하 관계가 깨짐. 이를 보존하기 위해 
    // 역의 전치 행렬(Transpose of Inverse) A = (M^-1)^T 를 유도하여 법선을 변환해야 함.
    bool GetNormalMatrix(float out_normal[3][3]) const {
        float a = m[0][0], b = m[0][1], c = m[0][2];
        float d = m[1][0], e = m[1][1], f = m[1][2];
        float g = m[2][0], h = m[2][1], i = m[2][2];

        float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
        if (std::abs(det) < 1e-9f) {
            return false;
        }
        float invdet = 1.0f / det;

        out_normal[0][0] = (e * i - f * h) * invdet;
        out_normal[0][1] = (f * g - d * i) * invdet;
        out_normal[0][2] = (d * h - e * g) * invdet;

        out_normal[1][0] = (c * h - b * i) * invdet;
        out_normal[1][1] = (a * i - c * g) * invdet;
        out_normal[1][2] = (b * g - a * h) * invdet;

        out_normal[2][0] = (b * f - c * e) * invdet;
        out_normal[2][1] = (c * d - a * f) * invdet;
        out_normal[2][2] = (a * e - b * d) * invdet;

        return true;
    }

    static Matrix4x4 Identity() {
        Matrix4x4 res;
        res.m[0][0] = 1.0f;
        res.m[1][1] = 1.0f;
        res.m[2][2] = 1.0f;
        res.m[3][3] = 1.0f;
        return res;
    }

    static Matrix4x4 Translation(float tx, float ty, float tz) {
        Matrix4x4 res = Identity();
        res.m[0][3] = tx;
        res.m[1][3] = ty;
        res.m[2][3] = tz;
        return res;
    }

    static Matrix4x4 Scaling(float sx, float sy, float sz) {
        Matrix4x4 res = Identity();
        res.m[0][0] = sx;
        res.m[1][1] = sy;
        res.m[2][2] = sz;
        return res;
    }

    static Matrix4x4 RotationX(float angle) {
        Matrix4x4 res = Identity();
        float c = std::cos(angle);
        float s = std::sin(angle);
        res.m[1][1] = c;
        res.m[1][2] = -s;
        res.m[2][1] = s;
        res.m[2][2] = c;
        return res;
    }

    static Matrix4x4 RotationY(float angle) {
        Matrix4x4 res = Identity();
        float c = std::cos(angle);
        float s = std::sin(angle);
        res.m[0][0] = c;
        res.m[0][2] = s;
        res.m[2][0] = -s;
        res.m[2][2] = c;
        return res;
    }

    static Matrix4x4 RotationZ(float angle) {
        Matrix4x4 res = Identity();
        float c = std::cos(angle);
        float s = std::sin(angle);
        res.m[0][0] = c;
        res.m[0][1] = -s;
        res.m[1][0] = s;
        res.m[1][1] = c;
        return res;
    }

    // [고급수학Ⅰ 세특 탐구 연계 - 오일러 변환 식 4.21]
    // 오일러 변환 E(h, p, r) = R_z(r) * R_x(p) * R_y(h) 계산.
    // 피치(p)가 90도 혹은 -90도인 경우 롤(r)과 헤드(h)의 회전축이 일치하여 
    // 3차원 공간에서 1차원의 회전 자유도를 유실하는 짐벌락(Gimbal Lock)이 수학적으로 발생하는 모델식.
    static Matrix4x4 EulerView(float heading, float pitch, float roll) {
        float h = heading;
        float p = pitch;
        float r = roll;

        float cos_h = std::cos(h), sin_h = std::sin(h);
        float cos_p = std::cos(p), sin_p = std::sin(p);
        float cos_r = std::cos(r), sin_r = std::sin(r);

        Matrix4x4 M;
        M.m[0][0] = cos_r * cos_h - sin_r * sin_p * sin_h;
        M.m[0][1] = -sin_r * cos_p;
        M.m[0][2] = cos_r * sin_h + sin_r * sin_p * cos_h;
        M.m[0][3] = 0.0f;

        M.m[1][0] = sin_r * cos_h + cos_r * sin_p * sin_h;
        M.m[1][1] = cos_r * cos_p;
        M.m[1][2] = sin_r * sin_h - cos_r * sin_p * cos_h;
        M.m[1][3] = 0.0f;

        M.m[2][0] = -cos_p * sin_h;
        M.m[2][1] = sin_p;
        M.m[2][2] = cos_p * cos_h;
        M.m[2][3] = 0.0f;

        M.m[3][0] = 0.0f;
        M.m[3][1] = 0.0f;
        M.m[3][2] = 0.0f;
        M.m[3][3] = 1.0f;

        return M;
    }
};
