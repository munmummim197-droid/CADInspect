#include <stepcompare/domain/fast_check.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

bool finite(double value) {
    return std::isfinite(value);
}

bool validStatistics(const stepcompare::domain::GeometryStatistics& statistics) {
    if (!finite(statistics.volumeMm3) || statistics.volumeMm3 < 0.0 ||
        !finite(statistics.surfaceAreaMm2) || statistics.surfaceAreaMm2 < 0.0) {
        return false;
    }
    for (const auto moment : statistics.principalInertia.moments) {
        if (!finite(moment) || moment < 0.0) {
            return false;
        }
    }
    return true;
}

bool exceedsRelativeTolerance(double a, double b, double relativeTolerance) {
    const auto scale = std::max({std::abs(a), std::abs(b), 1.0});
    return std::abs(a - b) > relativeTolerance * scale;
}

}  // namespace

namespace stepcompare::domain {

FastCheckResult compareFastInvariants(const GeometryStatistics& a,
                                      const GeometryStatistics& b,
                                      const ToleranceSet& tolerances) {
    if (!validStatistics(a) || !validStatistics(b) ||
        !finite(tolerances.relativeProperty) ||
        tolerances.relativeProperty < 0.0) {
        return {FastScreenStatus::InsufficientEvidence,
                {FastDifferenceReason::InvalidStatistics}};
    }

    std::vector<FastDifferenceReason> differences;
    if (exceedsRelativeTolerance(a.volumeMm3,
                                 b.volumeMm3,
                                 tolerances.relativeProperty)) {
        differences.push_back(FastDifferenceReason::Volume);
    }
    if (exceedsRelativeTolerance(a.surfaceAreaMm2,
                                 b.surfaceAreaMm2,
                                 tolerances.relativeProperty)) {
        differences.push_back(FastDifferenceReason::SurfaceArea);
    }
    if (a.topology != b.topology) {
        differences.push_back(FastDifferenceReason::Topology);
    }
    for (std::size_t index = 0; index < a.principalInertia.moments.size(); ++index) {
        if (exceedsRelativeTolerance(a.principalInertia.moments[index],
                                     b.principalInertia.moments[index],
                                     tolerances.relativeProperty)) {
            differences.push_back(FastDifferenceReason::PrincipalMoments);
            break;
        }
    }

    if (!differences.empty()) {
        return {FastScreenStatus::Different, std::move(differences)};
    }
    return {FastScreenStatus::CompatibleCandidate, {}};
}

}  // namespace stepcompare::domain
