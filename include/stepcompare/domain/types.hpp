#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace stepcompare::domain {

struct Vec3Mm final {
    double x{};
    double y{};
    double z{};

    [[nodiscard]] friend constexpr Vec3Mm operator-(const Vec3Mm& lhs,
                                                     const Vec3Mm& rhs) noexcept {
        return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    }
};

struct Quaternion final {
    double w{1.0};
    double x{};
    double y{};
    double z{};

    [[nodiscard]] Quaternion normalized() const {
        const auto magnitude = std::sqrt(w * w + x * x + y * y + z * z);
        if (magnitude <= 1.0e-15) {
            throw std::invalid_argument("Cannot normalize a zero quaternion");
        }
        return {w / magnitude, x / magnitude, y / magnitude, z / magnitude};
    }

    [[nodiscard]] constexpr Quaternion conjugate() const noexcept {
        return {w, -x, -y, -z};
    }
};

[[nodiscard]] constexpr Quaternion operator*(const Quaternion& lhs,
                                              const Quaternion& rhs) noexcept {
    return {
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
    };
}

struct RigidTransform final {
    Quaternion rotation{};
    Vec3Mm translation{};
};

struct ToleranceSet final {
    double positionMm{0.01};
    double surfaceMm{0.01};
    double angularDegrees{0.01};
    double booleanFuzzyMm{0.001};
    double relativeProperty{1.0e-6};
};

[[nodiscard]] constexpr Vec3Mm absoluteTranslationBMinusA(
    const Vec3Mm& positionA,
    const Vec3Mm& positionB) noexcept {
    return positionB - positionA;
}

[[nodiscard]] inline double relativeRotationAngleRadians(
    const Quaternion& orientationA,
    const Quaternion& orientationB) {
    const auto a = orientationA.normalized();
    const auto b = orientationB.normalized();
    const auto relative = (b * a.conjugate()).normalized();
    const auto canonicalW = std::clamp(std::abs(relative.w), 0.0, 1.0);
    return 2.0 * std::acos(canonicalW);
}

}  // namespace stepcompare::domain

