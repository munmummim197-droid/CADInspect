#pragma once

#include <stepcompare/domain/geometry.hpp>

#include <optional>
#include <vector>

namespace stepcompare::domain {

enum class PlacementSignalKind {
    CenterOfMass,
    BoundingBoxCenter,
    ComponentTransform,
    AssemblyTransform,
};

struct PlacementSignalDelta final {
    PlacementSignalKind kind{};
    Vec3Mm deltaBMinusA{};
};

struct PlacementInput final {
    GeometryStatistics a{};
    GeometryStatistics b{};
    std::optional<Vec3Mm> componentPositionA{};
    std::optional<Vec3Mm> componentPositionB{};
    std::optional<Vec3Mm> assemblyPositionA{};
    std::optional<Vec3Mm> assemblyPositionB{};
};

enum class PlacementAnalysisStatus {
    Same,
    Translated,
    ConflictingSignals,
    InsufficientEvidence,
};

struct PlacementAnalysis final {
    PlacementAnalysisStatus status{PlacementAnalysisStatus::InsufficientEvidence};
    Vec3Mm deltaBMinusA{};
    double maximumSignalDisagreementMm{};
    std::vector<PlacementSignalDelta> signals{};
};

[[nodiscard]] PlacementAnalysis analyzeAbsolutePlacement(
    const PlacementInput& input,
    const ToleranceSet& tolerances);

enum class SymmetryKind {
    None,
    Spherical,
    Axial,
};

struct RotationSymmetry final {
    SymmetryKind kind{SymmetryKind::None};
    UnitDirection axis{0.0, 0.0, 1.0};
};

enum class RotationAnalysisStatus {
    Same,
    Rotated,
    AmbiguousBySymmetry,
    Invalid,
};

struct RotationAnalysis final {
    RotationAnalysisStatus status{RotationAnalysisStatus::Invalid};
    double angleDegrees{};
    Quaternion relativeBFromA{};
};

[[nodiscard]] RotationAnalysis analyzeRotation(
    const Quaternion& orientationA,
    const Quaternion& orientationB,
    const RotationSymmetry& symmetry,
    const ToleranceSet& tolerances);

}  // namespace stepcompare::domain

