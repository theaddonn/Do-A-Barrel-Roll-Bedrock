#pragma once

#include <cmath>
#include <functional>

namespace do_a_barrel_roll::core {

inline constexpr double kPi    = 3.14159265358979323846;
inline constexpr double kToRad = kPi / 180.0;
inline constexpr double kToDeg = 180.0 / kPi;

struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    [[nodiscard]] double lengthSquared() const { return x * x + y * y; }
    [[nodiscard]] double length() const { return std::sqrt(lengthSquared()); }
    Vec2&                normalize();
    Vec2                 operator*(double s) const { return {x * s, y * s}; }
    Vec2                 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2                 operator-() const { return {-x, -y}; }
};

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3                 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3                 operator*(double s) const { return {x * s, y * s, z * s}; }
    [[nodiscard]] double dot(Vec3 o) const { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] Vec3   cross(Vec3 o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
    [[nodiscard]] double length() const { return std::sqrt(dot(*this)); }

    [[nodiscard]] Vec3 rotatedX(double a) const;
    [[nodiscard]] Vec3 rotatedY(double a) const;
    [[nodiscard]] Vec3 rotatedZ(double a) const;
    [[nodiscard]] Vec3 rotatedAxis(double angle, Vec3 axis) const;
};

[[nodiscard]] inline double lerp(double t, double a, double b) { return a + t * (b - a); }
[[nodiscard]] inline double clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
[[nodiscard]] inline double sign(double v) { return v > 0 ? 1.0 : (v < 0 ? -1.0 : 0.0); }

[[nodiscard]] Vec3 lookVector(double pitchDeg, double yawDeg);

void forBresenhamLine(int x0, int y0, int x1, int y1, const std::function<void(int, int)>& consumer);

struct LookResult {
    double pitch;
    double yaw;
    double roll;
    double deltaRoll;
};

[[nodiscard]] LookResult changeLookInLocalFrame(
    double currentPitchDeg,
    double currentYawDeg,
    double currentRollDeg,
    double pitchIn,
    double yawIn,
    double rollIn
);

}
