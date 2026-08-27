#include "feature_evidence.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace stepcompare::application {
namespace {

using feature::RecognizedFeature;
constexpr double kGeometryProvenMinimumConfidence = 0.80;

bool isIdentityAlignment(const deep::DeepGeometryResult& result) noexcept {
    if (!result.alignmentProven) {
        return false;
    }
    constexpr double tolerance = 1.0e-10;
    const import::RigidTransformMm identity;
    for (std::size_t index = 0; index < identity.matrix.size(); ++index) {
        if (std::abs(result.transformBToA.matrix[index] -
                     identity.matrix[index]) > tolerance) {
            return false;
        }
    }
    return true;
}

reporting::Vector3 reportVector(const feature::Vector3& value) noexcept {
    return {value.x, value.y, value.z};
}

feature::Vector3 transformPoint(const import::RigidTransformMm& transform,
                                const feature::Vector3& point) noexcept {
    const auto& m = transform.matrix;
    return {
        m[0] * point.x + m[1] * point.y + m[2] * point.z + m[3],
        m[4] * point.x + m[5] * point.y + m[6] * point.z + m[7],
        m[8] * point.x + m[9] * point.y + m[10] * point.z + m[11],
    };
}

feature::Vector3 transformDirection(
    const import::RigidTransformMm& transform,
    const feature::Vector3& direction) noexcept {
    const auto& m = transform.matrix;
    return {
        m[0] * direction.x + m[1] * direction.y + m[2] * direction.z,
        m[4] * direction.x + m[5] * direction.y + m[6] * direction.z,
        m[8] * direction.x + m[9] * direction.y + m[10] * direction.z,
    };
}

feature::Vector3 subtract(const feature::Vector3& right,
                          const feature::Vector3& left) noexcept {
    return {right.x - left.x, right.y - left.y, right.z - left.z};
}

double magnitude(const feature::Vector3& value) noexcept {
    return std::hypot(value.x, value.y, value.z);
}

double axisAngleDegrees(const feature::Vector3& first,
                        const feature::Vector3& second) noexcept {
    const double firstLength = magnitude(first);
    const double secondLength = magnitude(second);
    if (firstLength <= 0.0 || secondLength <= 0.0) {
        return 0.0;
    }
    const double dot = std::abs(
        (first.x * second.x + first.y * second.y + first.z * second.z) /
        (firstLength * secondLength));
    return std::acos(std::clamp(dot, 0.0, 1.0)) *
           180.0 / 3.141592653589793238462643383279502884;
}

const assembly::IndexedOccurrence* occurrenceFor(
    const assembly::AssemblyIndex& index,
    const std::string& id) noexcept {
    return id.empty() ? nullptr : index.findOccurrence(id);
}

const assembly::IndexedPrototype* prototypeFor(
    const assembly::AssemblyIndex& index,
    const assembly::IndexedOccurrence* occurrence) noexcept {
    return occurrence == nullptr
               ? nullptr
               : &index.prototypes[occurrence->prototypeIndex];
}

std::string featureId(const std::string& owner,
                      const RecognizedFeature& feature) {
    return owner + "/" + feature.stableId;
}

reporting::FeatureRow baseRow(
    const reporting::ComponentRow& component,
    const RecognizedFeature* featureA,
    const RecognizedFeature* featureB,
    const assembly::IndexedOccurrence* occurrenceA,
    const assembly::IndexedOccurrence* occurrenceB,
    const import::RigidTransformMm& alignmentBToA,
    const domain::ToleranceSet& tolerances) {
    reporting::FeatureRow row;
    row.ownerComponentIdA = component.idA;
    row.ownerComponentIdB = component.idB;
    row.idA = featureA == nullptr ? "" : featureId(component.idA, *featureA);
    row.idB = featureB == nullptr ? "" : featureId(component.idB, *featureB);
    row.type = feature::featureTypeName(
        featureA != nullptr ? featureA->type
                            : featureB != nullptr ? featureB->type
                                                  : feature::FeatureType::Unknown);
    row.positionToleranceMm = tolerances.positionMm;
    row.angularToleranceDegrees = tolerances.angularDegrees;

    if (featureA != nullptr) {
        row.primarySizeAMm = featureA->primarySizeMm;
        row.secondarySizeAMm = featureA->secondarySizeMm;
        row.depthAMm = featureA->depthMm;
        row.radiusAMm = featureA->radiusMm;
        row.angleADegrees = featureA->angleDegrees;
        row.profileA = featureA->profile;
        row.throughA = featureA->through;
        row.faceIndicesA = featureA->faceIndices;
        row.axisA = reportVector(featureA->axis);
        if (occurrenceA != nullptr) {
            row.centerAAbsoluteMm = reportVector(transformPoint(
                occurrenceA->worldTransform, featureA->centerLocalMm));
            row.axisA = reportVector(transformDirection(
                occurrenceA->worldTransform, featureA->axis));
        }
    }
    if (featureB != nullptr) {
        row.primarySizeBMm = featureB->primarySizeMm;
        row.secondarySizeBMm = featureB->secondarySizeMm;
        row.depthBMm = featureB->depthMm;
        row.radiusBMm = featureB->radiusMm;
        row.angleBDegrees = featureB->angleDegrees;
        row.profileB = featureB->profile;
        row.throughB = featureB->through;
        row.faceIndicesB = featureB->faceIndices;
        row.axisB = reportVector(featureB->axis);
        if (occurrenceB != nullptr) {
            row.centerBAbsoluteMm = reportVector(transformPoint(
                occurrenceB->worldTransform, featureB->centerLocalMm));
            row.axisB = reportVector(transformDirection(
                occurrenceB->worldTransform, featureB->axis));
        }
        if (occurrenceA != nullptr) {
            const auto alignedLocal =
                transformPoint(alignmentBToA, featureB->centerLocalMm);
            row.centerBAlignedMm = reportVector(transformPoint(
                occurrenceA->worldTransform, alignedLocal));
            row.axisBAligned = reportVector(transformDirection(
                occurrenceA->worldTransform,
                transformDirection(alignmentBToA, featureB->axis)));
        }
    }
    row.absoluteDifferenceBMinusAMm = {
        row.centerBAbsoluteMm.x - row.centerAAbsoluteMm.x,
        row.centerBAbsoluteMm.y - row.centerAAbsoluteMm.y,
        row.centerBAbsoluteMm.z - row.centerAAbsoluteMm.z,
    };
    row.alignedDifferenceBMinusAMm = {
        row.centerBAlignedMm.x - row.centerAAbsoluteMm.x,
        row.centerBAlignedMm.y - row.centerAAbsoluteMm.y,
        row.centerBAlignedMm.z - row.centerAAbsoluteMm.z,
    };
    row.confidence = std::min(featureA == nullptr ? 1.0 : featureA->confidence,
                              featureB == nullptr ? 1.0 : featureB->confidence);
    return row;
}

double matchScore(const RecognizedFeature& featureA,
                  const RecognizedFeature& featureB,
                  const import::RigidTransformMm& alignmentBToA) noexcept {
    const auto alignedB = transformPoint(alignmentBToA, featureB.centerLocalMm);
    const double center = magnitude(subtract(alignedB, featureA.centerLocalMm));
    return center + std::abs(featureB.primarySizeMm - featureA.primarySizeMm) +
           std::abs(featureB.secondarySizeMm - featureA.secondarySizeMm) +
           std::abs(featureB.depthMm - featureA.depthMm);
}

bool within(double difference, double tolerance) noexcept {
    return std::isfinite(difference) && std::abs(difference) <= tolerance;
}

void classify(reporting::FeatureRow& row,
               const RecognizedFeature* featureA,
               const RecognizedFeature* featureB,
               bool alignmentProven,
               const domain::ToleranceSet& tolerances) {
    const auto isProven = [](const RecognizedFeature* feature) {
        return feature == nullptr ||
               (feature->evidence ==
                    feature::RecognitionEvidence::GeometryProven &&
                feature->confidence >= kGeometryProvenMinimumConfidence);
    };
    if (!isProven(featureA) || !isProven(featureB)) {
        row.evidenceStatus = "FEATURE_AMBIGUOUS";
        row.result = "CHECK";
        row.reason = "FEATURE_AMBIGUOUS";
        return;
    }
    if (featureA == nullptr || featureB == nullptr) {
        row.evidenceStatus = "GEOMETRY_PROVEN";
        row.result = "FAIL";
        row.reason = featureA == nullptr ? "FEATURE_NEW" : "FEATURE_MISSING";
        return;
    }
    if (!alignmentProven) {
        row.evidenceStatus = "FEATURE_AMBIGUOUS";
        row.result = "CHECK";
        row.reason = "FEATURE_AMBIGUOUS";
        return;
    }
    if (featureA->type != featureB->type) {
        row.evidenceStatus = "GEOMETRY_PROVEN";
        row.result = "FAIL";
        row.reason = "FEATURE_TYPE_CHANGED";
        return;
    }

    const feature::Vector3 alignedDelta{
        row.alignedDifferenceBMinusAMm.x,
        row.alignedDifferenceBMinusAMm.y,
        row.alignedDifferenceBMinusAMm.z,
    };
    const feature::Vector3 axisA{row.axisA.x, row.axisA.y, row.axisA.z};
    const feature::Vector3 axisB{
        row.axisBAligned.x, row.axisBAligned.y, row.axisBAligned.z};
    row.evidenceStatus = "GEOMETRY_PROVEN";
    row.result = "FAIL";
    // Dimension changes can slightly perturb the best-fit rigid frame (for
    // example, a deeper pocket shifts the part COM). Report the directly
    // evidenced design dimension first; pure placement changes still fall
    // through to FEATURE_POSITION_CHANGED below.
    if (!within(row.primarySizeBMm - row.primarySizeAMm,
                       tolerances.surfaceMm) ||
               !within(row.secondarySizeBMm - row.secondarySizeAMm,
                       tolerances.surfaceMm)) {
        row.reason = "FEATURE_SIZE_CHANGED";
    } else if (!within(row.depthBMm - row.depthAMm,
                       tolerances.surfaceMm)) {
        row.reason = "FEATURE_DEPTH_CHANGED";
    } else if (!within(row.radiusBMm - row.radiusAMm,
                       tolerances.surfaceMm)) {
        row.reason = "FEATURE_RADIUS_CHANGED";
    } else if (!within(row.angleBDegrees - row.angleADegrees,
                       tolerances.angularDegrees)) {
        row.reason = "FEATURE_ANGLE_CHANGED";
    } else if (row.profileA != row.profileB) {
        row.reason = "FEATURE_PROFILE_CHANGED";
    } else if (magnitude(alignedDelta) > tolerances.positionMm) {
        row.reason = "FEATURE_POSITION_CHANGED";
    } else if (magnitude(axisA) <= 0.0 || magnitude(axisB) <= 0.0) {
        row.evidenceStatus = "FEATURE_AMBIGUOUS";
        row.result = "CHECK";
        row.reason = "FEATURE_AMBIGUOUS";
    } else if (axisAngleDegrees(axisA, axisB) >
               tolerances.angularDegrees) {
        row.reason = "FEATURE_ORIENTATION_CHANGED";
    } else if (row.throughA != row.throughB) {
        row.reason = "FEATURE_TYPE_CHANGED";
    } else {
        row.result = "PASS";
        row.reason = "FEATURE_SAME_AFTER_ALIGNMENT";
    }
}

}  // namespace

FeatureEvidenceStatus appendFeatureEvidence(
    const assembly::AssemblyIndex& indexA,
    const assembly::AssemblyIndex& indexB,
    const domain::ToleranceSet& tolerances,
    deep::DeepGeometryPort& deepGeometry,
    feature::FeatureRecognitionPort& recognizer,
    const bool exactIdentityProven,
    const std::unordered_map<std::string, deep::DeepGeometryResult>&
        precomputedAlignments,
    const std::stop_token cancellation,
    reporting::Report& report) noexcept {
    try {
        if (cancellation.stop_requested()) {
            return FeatureEvidenceStatus::Cancelled;
        }
        std::unordered_map<std::string, feature::FeatureRecognitionResult>
            recognizedA;
        std::unordered_map<std::string, feature::FeatureRecognitionResult>
            recognizedB;
        std::unordered_map<std::string, deep::DeepGeometryResult> alignments;

        for (const auto& component : report.components) {
            if (cancellation.stop_requested()) {
                return FeatureEvidenceStatus::Cancelled;
            }
            const auto* occurrenceA = occurrenceFor(indexA, component.idA);
            const auto* occurrenceB = occurrenceFor(indexB, component.idB);
            const auto* prototypeA = prototypeFor(indexA, occurrenceA);
            const auto* prototypeB = prototypeFor(indexB, occurrenceB);
            if (prototypeA == nullptr || prototypeB == nullptr) {
                continue;
            }

            auto [featuresA, insertedA] = recognizedA.try_emplace(prototypeA->id);
            if (insertedA) {
                featuresA->second = recognizer.recognize(
                    prototypeA->geometry,
                    tolerances.surfaceMm,
                    tolerances.angularDegrees,
                    cancellation);
                if (featuresA->second.cancelled ||
                    cancellation.stop_requested()) {
                    return FeatureEvidenceStatus::Cancelled;
                }
            }
            auto [featuresB, insertedB] = recognizedB.try_emplace(prototypeB->id);
            if (insertedB) {
                featuresB->second = recognizer.recognize(
                    prototypeB->geometry,
                    tolerances.surfaceMm,
                    tolerances.angularDegrees,
                    cancellation);
                if (featuresB->second.cancelled ||
                    cancellation.stop_requested()) {
                    return FeatureEvidenceStatus::Cancelled;
                }
            }
            if (featuresA->second.features.empty() &&
                featuresB->second.features.empty()) {
                continue;
            }

            import::RigidTransformMm alignmentBToA;
            bool alignmentProven = exactIdentityProven;
            const std::string pairKey = prototypeA->id + '\x1f' + prototypeB->id;
            if (!exactIdentityProven) {
                const deep::DeepGeometryResult* alignmentResult = nullptr;
                if (const auto precomputed =
                        precomputedAlignments.find(pairKey);
                    precomputed != precomputedAlignments.end() &&
                    isIdentityAlignment(precomputed->second)) {
                    // Identity is unique and cannot remap symmetric features.
                    // Non-identity whole-part alignments are recomputed in the
                    // feature stage so an arbitrary symmetry hypothesis is not
                    // reused as feature-placement evidence.
                    alignmentResult = &precomputed->second;
                } else {
                    auto [alignment, inserted] = alignments.try_emplace(pairKey);
                    if (inserted) {
                        alignment->second = deepGeometry.compareAligned({
                            prototypeA->geometry,
                            prototypeB->geometry,
                            {tolerances.booleanFuzzyMm,
                             tolerances.relativeProperty},
                        });
                    }
                    alignmentResult = &alignment->second;
                }
                if (cancellation.stop_requested()) {
                    return FeatureEvidenceStatus::Cancelled;
                }
                alignmentProven = alignmentResult->alignmentProven;
                if (alignmentProven) {
                    alignmentBToA = alignmentResult->transformBToA;
                }
            }

            const auto& listA = featuresA->second.features;
            const auto& listB = featuresB->second.features;
            std::vector<bool> usedB(listB.size(), false);
            for (const auto& featureA : listA) {
                if (cancellation.stop_requested()) {
                    return FeatureEvidenceStatus::Cancelled;
                }
                std::optional<std::size_t> best;
                double bestScore = std::numeric_limits<double>::infinity();
                for (std::size_t index = 0; index < listB.size(); ++index) {
                    if (usedB[index] || listB[index].type != featureA.type) {
                        continue;
                    }
                    const double score =
                        matchScore(featureA, listB[index], alignmentBToA);
                    if (score < bestScore) {
                        bestScore = score;
                        best = index;
                    }
                }
                const RecognizedFeature* featureB =
                    best ? &listB[*best] : nullptr;
                if (best) {
                    usedB[*best] = true;
                }
                auto row = baseRow(component,
                                   &featureA,
                                   featureB,
                                   occurrenceA,
                                   occurrenceB,
                                   alignmentBToA,
                                   tolerances);
                classify(row,
                         &featureA,
                         featureB,
                         alignmentProven,
                         tolerances);
                report.features.push_back(std::move(row));
            }
            for (std::size_t index = 0; index < listB.size(); ++index) {
                if (cancellation.stop_requested()) {
                    return FeatureEvidenceStatus::Cancelled;
                }
                if (usedB[index]) {
                    continue;
                }
                auto row = baseRow(component,
                                   nullptr,
                                   &listB[index],
                                   occurrenceA,
                                   occurrenceB,
                                   alignmentBToA,
                                   tolerances);
                classify(row,
                         nullptr,
                         &listB[index],
                         alignmentProven,
                         tolerances);
                report.features.push_back(std::move(row));
            }
        }
        return cancellation.stop_requested() ? FeatureEvidenceStatus::Cancelled
                                             : FeatureEvidenceStatus::Completed;
    } catch (...) {
        if (cancellation.stop_requested()) {
            return FeatureEvidenceStatus::Cancelled;
        }
        // Feature evidence is an additive report section. A recognizer failure
        // must never mutate the already-established whole-model verdict.
        reporting::FeatureRow row;
        row.type = "UNKNOWN";
        row.evidenceStatus = "FEATURE_AMBIGUOUS";
        row.result = "CHECK";
        row.reason = "FEATURE_RECOGNITION_FAILURE";
        row.positionToleranceMm = tolerances.positionMm;
        row.angularToleranceDegrees = tolerances.angularDegrees;
        report.features.push_back(std::move(row));
        return FeatureEvidenceStatus::Completed;
    }
}

}  // namespace stepcompare::application
