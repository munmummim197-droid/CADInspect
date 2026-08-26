#pragma once

#include <stepcompare/domain/geometry.hpp>

#include <vector>

namespace stepcompare::domain {

enum class FastScreenStatus {
    CompatibleCandidate,
    Different,
    InsufficientEvidence,
};

enum class FastDifferenceReason {
    InvalidStatistics,
    Volume,
    SurfaceArea,
    Topology,
    PrincipalMoments,
};

struct FastCheckResult final {
    FastScreenStatus status{FastScreenStatus::InsufficientEvidence};
    std::vector<FastDifferenceReason> reasons{};
};

// Candidate pruning only. CompatibleCandidate is never proof of equal geometry.
[[nodiscard]] FastCheckResult compareFastInvariants(
    const GeometryStatistics& a,
    const GeometryStatistics& b,
    const ToleranceSet& tolerances);

}  // namespace stepcompare::domain

