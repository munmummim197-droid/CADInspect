#include <stepcompare/domain/placement.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

using stepcompare::domain::Quaternion;
using stepcompare::domain::UnitDirection;
using stepcompare::domain::Vec3Mm;

double magnitude(double x, double y, double z) {
    return std::sqrt(x * x + y * y + z * z);
}

double distance(const Vec3Mm& a, const Vec3Mm& b) {
    return magnitude(a.x - b.x, a.y - b.y, a.z - b.z);
}

bool finite(const Vec3Mm& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool axisParallelTo(const Quaternion& rotation,
                    const UnitDirection& candidate,
                    double angularToleranceDegrees) {
    const auto rotationMagnitude = magnitude(rotation.x, rotation.y, rotation.z);
    const auto candidateMagnitude = magnitude(candidate.x, candidate.y, candidate.z);
    if (rotationMagnitude <= 1.0e-15 || candidateMagnitude <= 1.0e-15) {
        return false;
    }
    const auto dot = std::abs(
        (rotation.x * candidate.x + rotation.y * candidate.y +
         rotation.z * candidate.z) /
        (rotationMagnitude * candidateMagnitude));
    const auto toleranceRadians =
        angularToleranceDegrees * std::numbers::pi / 180.0;
    return dot >= std::cos(toleranceRadians);
}

}  // namespace

namespace stepcompare::domain {

PlacementAnalysis analyzeAbsolutePlacement(const PlacementInput& input,
                                           const ToleranceSet& tolerances) {
    PlacementAnalysis result;
    result.signals.push_back({
        PlacementSignalKind::CenterOfMass,
        absoluteTranslationBMinusA(input.a.centerOfMassMm,
                                   input.b.centerOfMassMm),
    });
    result.signals.push_back({
        PlacementSignalKind::BoundingBoxCenter,
        absoluteTranslationBMinusA(input.a.boundingBox.center(),
                                   input.b.boundingBox.center()),
    });
    if (input.componentPositionA && input.componentPositionB) {
        result.signals.push_back({
            PlacementSignalKind::ComponentTransform,
            absoluteTranslationBMinusA(*input.componentPositionA,
                                       *input.componentPositionB),
        });
    }
    if (input.assemblyPositionA && input.assemblyPositionB) {
        result.signals.push_back({
            PlacementSignalKind::AssemblyTransform,
            absoluteTranslationBMinusA(*input.assemblyPositionA,
                                       *input.assemblyPositionB),
        });
    }

    if (!std::isfinite(tolerances.positionMm) || tolerances.positionMm < 0.0) {
        return result;
    }
    for (const auto& signal : result.signals) {
        if (!finite(signal.deltaBMinusA)) {
            return result;
        }
        result.deltaBMinusA.x += signal.deltaBMinusA.x;
        result.deltaBMinusA.y += signal.deltaBMinusA.y;
        result.deltaBMinusA.z += signal.deltaBMinusA.z;
    }
    const auto count = static_cast<double>(result.signals.size());
    result.deltaBMinusA.x /= count;
    result.deltaBMinusA.y /= count;
    result.deltaBMinusA.z /= count;

    for (const auto& signal : result.signals) {
        result.maximumSignalDisagreementMm = std::max(
            result.maximumSignalDisagreementMm,
            distance(signal.deltaBMinusA, result.deltaBMinusA));
    }
    if (result.maximumSignalDisagreementMm > tolerances.positionMm) {
        result.status = PlacementAnalysisStatus::ConflictingSignals;
        return result;
    }

    const auto translationMagnitude = magnitude(result.deltaBMinusA.x,
                                                 result.deltaBMinusA.y,
                                                 result.deltaBMinusA.z);
    result.status = translationMagnitude <= tolerances.positionMm
                        ? PlacementAnalysisStatus::Same
                        : PlacementAnalysisStatus::Translated;
    return result;
}

RotationAnalysis analyzeRotation(const Quaternion& orientationA,
                                 const Quaternion& orientationB,
                                 const RotationSymmetry& symmetry,
                                 const ToleranceSet& tolerances) {
    RotationAnalysis result;
    if (!std::isfinite(tolerances.angularDegrees) ||
        tolerances.angularDegrees < 0.0) {
        return result;
    }

    try {
        const auto a = orientationA.normalized();
        const auto b = orientationB.normalized();
        result.relativeBFromA = (b * a.conjugate()).normalized();
    } catch (const std::invalid_argument&) {
        return result;
    }

    if (result.relativeBFromA.w < 0.0) {
        result.relativeBFromA.w = -result.relativeBFromA.w;
        result.relativeBFromA.x = -result.relativeBFromA.x;
        result.relativeBFromA.y = -result.relativeBFromA.y;
        result.relativeBFromA.z = -result.relativeBFromA.z;
    }
    const auto canonicalW = std::clamp(result.relativeBFromA.w, 0.0, 1.0);
    result.angleDegrees =
        2.0 * std::acos(canonicalW) * 180.0 / std::numbers::pi;

    if (result.angleDegrees <= tolerances.angularDegrees) {
        result.status = RotationAnalysisStatus::Same;
    } else if (symmetry.kind == SymmetryKind::Spherical ||
               (symmetry.kind == SymmetryKind::Axial &&
                axisParallelTo(result.relativeBFromA,
                               symmetry.axis,
                               tolerances.angularDegrees))) {
        result.status = RotationAnalysisStatus::AmbiguousBySymmetry;
    } else {
        result.status = RotationAnalysisStatus::Rotated;
    }
    return result;
}

}  // namespace stepcompare::domain
