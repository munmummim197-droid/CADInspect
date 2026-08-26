#pragma once

#include <stepcompare/domain/types.hpp>

#include <array>
#include <cstdint>

namespace stepcompare::domain {

struct BoundingBoxMm final {
    Vec3Mm minimum{};
    Vec3Mm maximum{};

    [[nodiscard]] constexpr Vec3Mm center() const noexcept {
        return {
            (minimum.x + maximum.x) * 0.5,
            (minimum.y + maximum.y) * 0.5,
            (minimum.z + maximum.z) * 0.5,
        };
    }

    [[nodiscard]] constexpr Vec3Mm size() const noexcept {
        return maximum - minimum;
    }
};

struct TopologyCounts final {
    std::uint64_t solids{};
    std::uint64_t shells{};
    std::uint64_t faces{};
    std::uint64_t edges{};
    std::uint64_t vertices{};

    [[nodiscard]] friend constexpr bool operator==(
        const TopologyCounts&,
        const TopologyCounts&) noexcept = default;
};

struct UnitDirection final {
    double x{};
    double y{};
    double z{};
};

struct PrincipalInertia final {
    std::array<double, 3> moments{};
    std::array<UnitDirection, 3> axes{};
};

struct GeometryStatistics final {
    BoundingBoxMm boundingBox{};
    double volumeMm3{};
    double surfaceAreaMm2{};
    Vec3Mm centerOfMassMm{};
    TopologyCounts topology{};
    PrincipalInertia principalInertia{};
    bool closedSolidEvidence{false};
};

}  // namespace stepcompare::domain

