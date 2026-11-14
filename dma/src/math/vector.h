#pragma once
#include <cmath>
#include <cstdint>

struct FVectorDouble {
    double x = 0.0, y = 0.0, z = 0.0;
};

struct FRotatorDouble {
    double pitch = 0.0, yaw = 0.0, roll = 0.0;
};

struct Vector2 {
    float x = 0.f, y = 0.f;

    Vector2() = default;
    Vector2(float x, float y) : x(x), y(y) {}

    Vector2 operator+(const Vector2& v) const { return { x + v.x, y + v.y }; }
    Vector2 operator-(const Vector2& v) const { return { x - v.x, y - v.y }; }
    Vector2 operator*(float s) const { return { x * s, y * s }; }

    float Length() const { return std::sqrtf(x * x + y * y); }
    float Distance(const Vector2& v) const { return (*this - v).Length(); }
};

struct Vector3 {
    float x = 0.f, y = 0.f, z = 0.f;

    Vector3() = default;
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vector3 operator+(const Vector3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    Vector3 operator-(const Vector3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    Vector3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vector3 operator/(float s) const { return { x / s, y / s, z / s }; }

    float Length() const { return std::sqrtf(x * x + y * y + z * z); }
    float Distance(const Vector3& v) const { return (*this - v).Length(); }

    float Dot(const Vector3& v) const {
        return x * v.x + y * v.y + z * v.z;
    }

    Vector3 Cross(const Vector3& v) const {
        return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x };
    }

    Vector3 Normalized() const {
        float len = Length();
        if (len < 0.0001f) return {};
        return *this / len;
    }

    bool IsZero() const {
        return x == 0.f && y == 0.f && z == 0.f;
    }

    bool IsValid() const {
        return !std::isnan(x) && !std::isnan(y) && !std::isnan(z) &&
               !std::isinf(x) && !std::isinf(y) && !std::isinf(z);
    }
};

struct FRotator {
    float pitch = 0.f, yaw = 0.f, roll = 0.f;

    FRotator() = default;
    FRotator(float p, float y, float r) : pitch(p), yaw(y), roll(r) {}

    Vector3 ToDirection() const {
        float sp, cp, sy, cy;
        constexpr float DEG2RAD = 3.14159265358979323846f / 180.f;
        sp = std::sinf(pitch * DEG2RAD);
        cp = std::cosf(pitch * DEG2RAD);
        sy = std::sinf(yaw * DEG2RAD);
        cy = std::cosf(yaw * DEG2RAD);
        return { cp * cy, cp * sy, -sp };
    }
};

struct FMinimalViewInfo {
    Vector3 location;
    FRotator rotation;
    float fov = 90.f;
    // Camera axes taken from the game orientation. Rebuilding them from
    // yaw and pitch drops the shoulder roll and shifts boxes sideways.
    Vector3 axisRight;
    Vector3 axisUp;
    Vector3 axisForward;
    bool hasAxes = false;
};

struct Matrix4x4 {
    float m[4][4] = {};

    static Matrix4x4 Identity() {
        Matrix4x4 mat;
        mat.m[0][0] = mat.m[1][1] = mat.m[2][2] = mat.m[3][3] = 1.f;
        return mat;
    }

    Vector3 TransformPosition(const Vector3& v) const {
        float w = m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3];
        if (std::fabsf(w) < 0.0001f) w = 0.0001f;
        return {
            (m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3]) / w,
            (m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3]) / w,
            (m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3]) / w
        };
    }
};

// WorldToScreen using a 4x4 view-projection matrix (from Unity Camera).
// This is the preferred method — no manual rotation math needed.
// Matrix is row-major as stored by Unity (m[row][col]).
inline bool WorldToScreenMatrix(const Vector3& worldPos, const float vm[16],
                                int screenWidth, int screenHeight,
                                Vector2& screenOut)
{
    float w = vm[3] * worldPos.x + vm[7] * worldPos.y + vm[11] * worldPos.z + vm[15];
    if (w < 0.001f) return false;

    float x = vm[0] * worldPos.x + vm[4] * worldPos.y + vm[8]  * worldPos.z + vm[12];
    float y = vm[1] * worldPos.x + vm[5] * worldPos.y + vm[9]  * worldPos.z + vm[13];

    screenOut.x = (1.f + x / w) * 0.5f * screenWidth;
    screenOut.y = (1.f - y / w) * 0.5f * screenHeight;
    return true;
}

// Unity is Y-up. Yaw 0 looks along +Z and increases toward +X.
// Pitch is degrees, positive looks up. camera.location is in Unity space.
inline bool WorldToScreenUnity(const Vector3& worldPos, const FMinimalViewInfo& camera,
                               int screenWidth, int screenHeight,
                               Vector2& screenOut)
{
    if (!worldPos.IsValid() || !camera.location.IsValid() ||
        !std::isfinite(camera.rotation.yaw) ||
        !std::isfinite(camera.rotation.pitch) ||
        !std::isfinite(camera.fov) ||
        screenWidth <= 0 || screenHeight <= 0)
        return false;

    constexpr float DEG2RAD = 3.14159265358979323846f / 180.f;
    Vector3 forward;
    Vector3 right;
    Vector3 up;
    if (camera.hasAxes &&
        camera.axisForward.Length() > 0.5f &&
        camera.axisRight.Length() > 0.5f &&
        camera.axisUp.Length() > 0.5f) {
        forward = camera.axisForward.Normalized();
        right = camera.axisRight.Normalized();
        up = camera.axisUp.Normalized();
    } else {
        float yaw = camera.rotation.yaw * DEG2RAD;
        float pitch = camera.rotation.pitch * DEG2RAD;
        forward = {
            std::sinf(yaw) * std::cosf(pitch),
            std::sinf(pitch),
            std::cosf(yaw) * std::cosf(pitch)
        };
        Vector3 worldUp = { 0.f, 1.f, 0.f };
        right = worldUp.Cross(forward).Normalized();
        up = forward.Cross(right);
    }

    Vector3 delta = worldPos - camera.location;
    float x = delta.Dot(right);
    float y = delta.Dot(up);
    float z = delta.Dot(forward);
    if (z < 0.04f)
        return false;

    float tanHalfFov = std::tanf(camera.fov * DEG2RAD * 0.5f);
    if (tanHalfFov < 0.001f)
        return false;
    float aspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
    float cx = screenWidth * 0.5f;
    float cy = screenHeight * 0.5f;
    screenOut.x = cx + (x / (tanHalfFov * aspect * z)) * cx;
    screenOut.y = cy - (y / (tanHalfFov * z)) * cy;
    return std::isfinite(screenOut.x) && std::isfinite(screenOut.y);
}

// Fallback WorldToScreen using rotation angles (degrees) and FOV
inline bool WorldToScreen(const Vector3& worldPos, const FMinimalViewInfo& camera,
                          int screenWidth, int screenHeight,
                          Vector2& screenOut)
{
    constexpr float DEG2RAD = 3.14159265358979323846f / 180.f;

    Vector3 delta = worldPos - camera.location;

    float sp = std::sinf(camera.rotation.pitch * DEG2RAD);
    float cp = std::cosf(camera.rotation.pitch * DEG2RAD);
    float sy = std::sinf(camera.rotation.yaw * DEG2RAD);
    float cy = std::cosf(camera.rotation.yaw * DEG2RAD);
    float sr = std::sinf(camera.rotation.roll * DEG2RAD);
    float cr = std::cosf(camera.rotation.roll * DEG2RAD);

    Vector3 axisX = { cp * cy, cp * sy, sp };
    Vector3 axisY = { sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp };
    Vector3 axisZ = { -(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp };

    Vector3 transformed = {
        delta.Dot(axisY),
        delta.Dot(axisZ),
        delta.Dot(axisX)
    };

    if (transformed.z < 0.1f)
        return false;

    float fovRad = camera.fov * DEG2RAD;
    float screenCenterX = screenWidth / 2.f;
    float screenCenterY = screenHeight / 2.f;
    float tanHalfFov = std::tanf(fovRad / 2.f);
    float aspectRatio = static_cast<float>(screenWidth) / screenHeight;

    screenOut.x = screenCenterX + (transformed.x / (tanHalfFov * aspectRatio * transformed.z)) * screenCenterX;
    screenOut.y = screenCenterY - (transformed.y / (tanHalfFov * transformed.z)) * screenCenterY;

    return true;
}
