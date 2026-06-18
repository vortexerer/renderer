#pragma once

#include <cmath>

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    // Operators
    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }
    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }
    Vector3 operator*(float scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }
    friend Vector3 operator*(float scalar, const Vector3& vec) {
        return Vector3(vec.x * scalar, vec.y * scalar, vec.z * scalar);
    }
    Vector3 operator/(float scalar) const {
        if (std::abs(scalar) < 1e-9f) {
            return Vector3(0.0f, 0.0f, 0.0f);
        }
        return Vector3(x / scalar, y / scalar, z / scalar);
    }

    Vector3& operator+=(const Vector3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    Vector3& operator-=(const Vector3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    Vector3& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
    Vector3& operator/=(float scalar) {
        if (std::abs(scalar) < 1e-9f) {
            x = y = z = 0.0f;
        } else {
            x /= scalar;
            y /= scalar;
            z /= scalar;
        }
        return *this;
    }

    bool operator==(const Vector3& other) const {
        return std::abs(x - other.x) < 1e-5f &&
               std::abs(y - other.y) < 1e-5f &&
               std::abs(z - other.z) < 1e-5f;
    }
    bool operator!=(const Vector3& other) const {
        return !(*this == other);
    }

    // Unary minus
    Vector3 operator-() const {
        return Vector3(-x, -y, -z);
    }

    // Methods
    float LengthSquared() const {
        return x * x + y * y + z * z;
    }
    float Length() const {
        return std::sqrt(LengthSquared());
    }
    Vector3 Normalize() const {
        float len = Length();
        if (len < 1e-9f) {
            return Vector3(0.0f, 0.0f, 0.0f);
        }
        return *this / len;
    }

    float Dot(const Vector3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    Vector3 Cross(const Vector3& other) const {
        return Vector3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
};
