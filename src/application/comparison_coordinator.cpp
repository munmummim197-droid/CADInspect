#include "stepcompare/application/comparison_coordinator.hpp"

#include "stepcompare/assembly/assembly_index.hpp"
#include "stepcompare/assembly/component_matching.hpp"
#include "stepcompare/domain/fast_check.hpp"
#include "stepcompare/domain/placement.hpp"
#include "stepcompare/cache/cache_key.hpp"
#include "feature_evidence.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace stepcompare::application {
namespace {

using Clock = std::chrono::steady_clock;

using assembly::AssemblyIndex;
using assembly::AssemblyMatchResult;
using assembly::ComponentMatchRow;
using assembly::ComponentResultStatus;
using assembly::GeometryEvidence;
using assembly::MatchStatus;

std::string utf8Bytes(const std::u8string& value) {
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

reporting::Vector3 reportVector(const domain::Vec3Mm& value) noexcept {
    return {value.x, value.y, value.z};
}

reporting::Vector3 reportDirection(const domain::UnitDirection& value) noexcept {
    return {value.x, value.y, value.z};
}

reporting::GeometryStatistics reportStatistics(
    const domain::GeometryStatistics& value) {
    reporting::GeometryStatistics result;
    result.boundingBoxMinimumMm = reportVector(value.boundingBox.minimum);
    result.boundingBoxMaximumMm = reportVector(value.boundingBox.maximum);
    result.sizeMm = reportVector(value.boundingBox.size());
    result.volumeMm3 = value.volumeMm3;
    result.surfaceAreaMm2 = value.surfaceAreaMm2;
    result.centerOfMassMm = reportVector(value.centerOfMassMm);
    result.solidCount = value.topology.solids;
    result.shellCount = value.topology.shells;
    result.faceCount = value.topology.faces;
    result.edgeCount = value.topology.edges;
    result.vertexCount = value.topology.vertices;
    result.principalMoments = {value.principalInertia.moments[0],
                               value.principalInertia.moments[1],
                               value.principalInertia.moments[2]};
    for (std::size_t index = 0; index < value.principalInertia.axes.size();
         ++index) {
        result.principalAxes[index] =
            reportDirection(value.principalInertia.axes[index]);
    }
    return result;
}

domain::GeometryStatistics aggregateStatistics(const AssemblyIndex& index) {
    if (index.prototypes.empty()) {
        return {};
    }
    if (index.prototypes.size() == 1U) {
        return index.prototypes.front().statistics;
    }

    domain::GeometryStatistics result;
    result.boundingBox.minimum = {
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
    };
    result.boundingBox.maximum = {
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };
    for (const auto& prototype : index.prototypes) {
        const auto& statistics = prototype.statistics;
        result.volumeMm3 += statistics.volumeMm3;
        result.surfaceAreaMm2 += statistics.surfaceAreaMm2;
        result.topology.solids += statistics.topology.solids;
        result.topology.shells += statistics.topology.shells;
        result.topology.faces += statistics.topology.faces;
        result.topology.edges += statistics.topology.edges;
        result.topology.vertices += statistics.topology.vertices;
        result.boundingBox.minimum.x = std::min(
            result.boundingBox.minimum.x, statistics.boundingBox.minimum.x);
        result.boundingBox.minimum.y = std::min(
            result.boundingBox.minimum.y, statistics.boundingBox.minimum.y);
        result.boundingBox.minimum.z = std::min(
            result.boundingBox.minimum.z, statistics.boundingBox.minimum.z);
        result.boundingBox.maximum.x = std::max(
            result.boundingBox.maximum.x, statistics.boundingBox.maximum.x);
        result.boundingBox.maximum.y = std::max(
            result.boundingBox.maximum.y, statistics.boundingBox.maximum.y);
        result.boundingBox.maximum.z = std::max(
            result.boundingBox.maximum.z, statistics.boundingBox.maximum.z);
    }
    // COM and principal inertia are intentionally not synthesized across
    // unplaced prototypes; component rows carry trustworthy occurrence data.
    return result;
}

std::string decisionName(domain::Decision value) {
    switch (value) {
    case domain::Decision::Pass:
        return "PASS";
    case domain::Decision::Fail:
        return "FAIL";
    case domain::Decision::Check:
        return "CHECK";
    case domain::Decision::Error:
        return "ERROR";
    }
    return "ERROR";
}

std::string reasonName(domain::ReasonCode value) {
    switch (value) {
    case domain::ReasonCode::SameGeometrySamePosition:
        return "SAME_GEOMETRY_SAME_POSITION";
    case domain::ReasonCode::SameGeometryPositionChanged:
        return "SAME_GEOMETRY_POSITION_CHANGED";
    case domain::ReasonCode::GeometryChanged:
        return "GEOMETRY_CHANGED";
    case domain::ReasonCode::ComponentMissing:
        return "COMPONENT_MISSING";
    case domain::ReasonCode::ComponentAdded:
        return "COMPONENT_ADDED";
    case domain::ReasonCode::AlignmentAmbiguous:
        return "ALIGNMENT_AMBIGUOUS";
    case domain::ReasonCode::RotationAmbiguousBySymmetry:
        return "ROTATION_AMBIGUOUS_BY_SYMMETRY";
    case domain::ReasonCode::EvidenceIncomplete:
        return "EVIDENCE_INCOMPLETE";
    case domain::ReasonCode::StepImportFailed:
        return "STEP_IMPORT_FAILED";
    case domain::ReasonCode::DeepCheckFailed:
        return "DEEP_CHECK_FAILED";
    }
    return "EVIDENCE_INCOMPLETE";
}

void applyVerdict(reporting::Report& report, const domain::Verdict& verdict) {
    report.verdict.decision = decisionName(verdict.decision);
    report.verdict.reasons.clear();
    for (const auto reason : verdict.reasons) {
        report.verdict.reasons.push_back(reasonName(reason));
    }
}

std::string matchStatusName(MatchStatus value) {
    switch (value) {
    case MatchStatus::MatchExact:
        return "MATCH_EXACT";
    case MatchStatus::MatchGeometry:
        return "MATCH_GEOMETRY";
    case MatchStatus::MatchProbable:
        return "MATCH_PROBABLE";
    case MatchStatus::Ambiguous:
        return "AMBIGUOUS";
    case MatchStatus::NotMatched:
        return "NOT_MATCHED";
    }
    return "AMBIGUOUS";
}

std::string geometryEvidenceName(GeometryEvidence value) {
    switch (value) {
    case GeometryEvidence::SameProven:
        return "SAME_PROVEN";
    case GeometryEvidence::DifferentProven:
        return "DIFFERENT_PROVEN";
    case GeometryEvidence::Inconclusive:
        return "INCONCLUSIVE";
    }
    return "INCONCLUSIVE";
}

std::string componentStatusName(ComponentResultStatus value) {
    switch (value) {
    case ComponentResultStatus::Same:
        return "SAME";
    case ComponentResultStatus::Moved:
        return "MOVED";
    case ComponentResultStatus::Rotated:
        return "ROTATED";
    case ComponentResultStatus::MovedAndRotated:
        return "MOVED_AND_ROTATED";
    case ComponentResultStatus::GeometryChanged:
        return "GEOMETRY_CHANGED";
    case ComponentResultStatus::Missing:
        return "MISSING";
    case ComponentResultStatus::New:
        return "NEW";
    case ComponentResultStatus::Ambiguous:
        return "AMBIGUOUS";
    case ComponentResultStatus::Check:
        return "CHECK";
    }
    return "CHECK";
}

std::string positionStatusName(ComponentResultStatus value) {
    switch (value) {
    case ComponentResultStatus::Same:
        return "SAME";
    case ComponentResultStatus::Moved:
        return "MOVED";
    case ComponentResultStatus::Rotated:
        return "ROTATED";
    case ComponentResultStatus::MovedAndRotated:
        return "MOVED_AND_ROTATED";
    case ComponentResultStatus::GeometryChanged:
    case ComponentResultStatus::Missing:
    case ComponentResultStatus::New:
    case ComponentResultStatus::Ambiguous:
    case ComponentResultStatus::Check:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

const assembly::IndexedPrototype* prototypeFor(
    const AssemblyIndex& index,
    const std::optional<std::string>& nodeId) {
    if (!nodeId) {
        return nullptr;
    }
    const auto* occurrence = index.findOccurrence(*nodeId);
    return occurrence == nullptr
               ? nullptr
               : &index.prototypes[occurrence->prototypeIndex];
}

const assembly::IndexedOccurrence* occurrenceFor(
    const AssemblyIndex& index,
    const std::optional<std::string>& nodeId) {
    return nodeId ? index.findOccurrence(*nodeId) : nullptr;
}

reporting::ComponentRow reportComponent(const ComponentMatchRow& row,
                                        const AssemblyIndex& indexA,
                                        const AssemblyIndex& indexB) {
    reporting::ComponentRow result;
    result.idA = row.nodeIdA.value_or("");
    result.idB = row.nodeIdB.value_or("");
    result.matchStatus = matchStatusName(row.matchStatus);
    result.geometryStatus = geometryEvidenceName(row.geometryEvidence);
    result.positionStatus = positionStatusName(row.resultStatus);
    result.translationBMinusAMm = reportVector(row.translationBMinusAMm);
    result.rotationAngleDegrees = row.rotationAngleDegrees;
    result.confidence = row.confidence;

    const auto* occurrenceA = occurrenceFor(indexA, row.nodeIdA);
    const auto* occurrenceB = occurrenceFor(indexB, row.nodeIdB);
    if (occurrenceA != nullptr) {
        result.nameA = occurrenceA->nameUtf8;
    }
    if (occurrenceB != nullptr) {
        result.nameB = occurrenceB->nameUtf8;
    }
    const auto* prototypeA = prototypeFor(indexA, row.nodeIdA);
    const auto* prototypeB = prototypeFor(indexB, row.nodeIdB);
    if (prototypeA != nullptr && prototypeB != nullptr) {
        const auto sizeA = prototypeA->statistics.boundingBox.size();
        const auto sizeB = prototypeB->statistics.boundingBox.size();
        result.boundingBoxSizeDifferenceMm = {
            sizeB.x - sizeA.x, sizeB.y - sizeA.y, sizeB.z - sizeA.z};
        result.volumeDifferenceMm3 = prototypeB->statistics.volumeMm3 -
                                     prototypeA->statistics.volumeMm3;
        result.surfaceAreaDifferenceMm2 =
            prototypeB->statistics.surfaceAreaMm2 -
            prototypeA->statistics.surfaceAreaMm2;
    }
    return result;
}

reporting::Quaternion quaternionFromTransform(
    const import::RigidTransformMm& transform) noexcept {
    const auto& m = transform.matrix;
    reporting::Quaternion result;
    const double trace = m[0] + m[5] + m[10];
    if (trace > 0.0) {
        const double scale = 2.0 * std::sqrt(trace + 1.0);
        result.w = 0.25 * scale;
        result.x = (m[9] - m[6]) / scale;
        result.y = (m[2] - m[8]) / scale;
        result.z = (m[4] - m[1]) / scale;
    } else if (m[0] > m[5] && m[0] > m[10]) {
        const double scale = 2.0 * std::sqrt(1.0 + m[0] - m[5] - m[10]);
        result.w = (m[9] - m[6]) / scale;
        result.x = 0.25 * scale;
        result.y = (m[1] + m[4]) / scale;
        result.z = (m[2] + m[8]) / scale;
    } else if (m[5] > m[10]) {
        const double scale = 2.0 * std::sqrt(1.0 + m[5] - m[0] - m[10]);
        result.w = (m[2] - m[8]) / scale;
        result.x = (m[1] + m[4]) / scale;
        result.y = 0.25 * scale;
        result.z = (m[6] + m[9]) / scale;
    } else {
        const double scale = 2.0 * std::sqrt(1.0 + m[10] - m[0] - m[5]);
        result.w = (m[4] - m[1]) / scale;
        result.x = (m[2] + m[8]) / scale;
        result.y = (m[6] + m[9]) / scale;
        result.z = 0.25 * scale;
    }
    return result;
}

double rotationAngleDegrees(
    const import::RigidTransformMm& transform) noexcept {
    const double cosine = std::clamp(
        (transform.matrix[0] + transform.matrix[5] + transform.matrix[10] -
         1.0) *
            0.5,
        -1.0,
        1.0);
    constexpr double radiansToDegrees =
        180.0 / 3.141592653589793238462643383279502884;
    return std::acos(cosine) * radiansToDegrees;
}

double relativeRotationAngleDegrees(
    const import::RigidTransformMm& transformA,
    const import::RigidTransformMm& transformB) noexcept {
    double relativeTrace = 0.0;
    for (std::size_t row = 0; row < 3U; ++row) {
        for (std::size_t column = 0; column < 3U; ++column) {
            relativeTrace += transformA.matrix[row * 4U + column] *
                             transformB.matrix[row * 4U + column];
        }
    }
    const double cosine =
        std::clamp((relativeTrace - 1.0) * 0.5, -1.0, 1.0);
    constexpr double radiansToDegrees =
        180.0 / 3.141592653589793238462643383279502884;
    return std::acos(cosine) * radiansToDegrees;
}

domain::PositionStatus positionStatus(bool moved, bool rotated) noexcept {
    if (moved && rotated) {
        return domain::PositionStatus::TranslatedAndRotated;
    }
    if (moved) {
        return domain::PositionStatus::Translated;
    }
    if (rotated) {
        return domain::PositionStatus::Rotated;
    }
    return domain::PositionStatus::Same;
}

void initializeReport(reporting::Report& report,
                      const ComparisonRequest& request) {
    report.softwareVersion = STEPCOMPARE_SOFTWARE_VERSION;
    report.algorithmVersion = "dev-v1";
    report.inputA.pathUtf8 = utf8Bytes(request.inputAUtf8);
    report.inputB.pathUtf8 = utf8Bytes(request.inputBUtf8);
    if (request.identityA) {
        report.inputA.sha256 = request.identityA->sha256Hex;
        report.inputA.sizeBytes =
            static_cast<std::uint64_t>(request.identityA->sizeBytes);
    }
    if (request.identityB) {
        report.inputB.sha256 = request.identityB->sha256Hex;
        report.inputB.sizeBytes =
            static_cast<std::uint64_t>(request.identityB->sizeBytes);
    }
    report.tolerances.positionMm = request.tolerances.positionMm;
    report.tolerances.surfaceMm = request.tolerances.surfaceMm;
    report.tolerances.angularDegrees = request.tolerances.angularDegrees;
    report.tolerances.booleanFuzzyMm = request.tolerances.booleanFuzzyMm;
    report.tolerances.relativeProperty = request.tolerances.relativeProperty;
    report.execution.status = "RUNNING";
    report.execution.terminalPhase = "INITIALIZE";
}

reporting::DeviationStatistics reportDeviation(
    const deviation::SurfaceDeviationResult& value) noexcept {
    reporting::DeviationStatistics result;
    result.available =
        value.status == deviation::SurfaceDeviationStatus::WithinTolerance ||
        value.status == deviation::SurfaceDeviationStatus::DeviationFound;
    if (result.available) {
        result.maximumMm = value.maximumMm;
        result.meanMm = value.meanMm;
        result.rmsMm = value.rmsMm;
        result.percentile95Mm = value.percentileMm;
        result.sampleCount = static_cast<std::uint64_t>(
            value.samplesAToB + value.samplesBToA);
        result.triangleDistanceEvaluations = static_cast<std::uint64_t>(
            value.triangleDistanceEvaluations);
    }
    return result;
}

std::optional<deviation::SurfaceDeviationResult> runDeviation(
    const ComparisonRequest& request,
    deviation::SurfaceDeviationPort* port,
    const import::GeometryPayloadPtr& geometryA,
    const import::GeometryPayloadPtr& geometryB,
    const import::RigidTransformMm& transformBToA) {
    if (port == nullptr || !request.deep) {
        return std::nullopt;
    }
    deviation::SurfaceDeviationRequest deviationRequest;
    deviationRequest.geometryA = geometryA;
    deviationRequest.geometryB = geometryB;
    deviationRequest.transformBToA = transformBToA;
    deviationRequest.options.toleranceMm = request.tolerances.surfaceMm;
    deviationRequest.options.meshDeflectionMm =
        std::max(request.tolerances.surfaceMm * 0.5, 0.001);
    deviationRequest.isCancelled = [&request] {
        return request.cancellation.stop_requested();
    };
    return port->compare(deviationRequest);
}

void notifyProgress(const ComparisonRequest& request,
                    ComparisonPhase phase,
                    std::size_t completedStages,
                    std::size_t totalStages = 5U) noexcept {
    if (!request.progress) {
        return;
    }
    try {
        request.progress({phase, completedStages, totalStages});
    } catch (...) {
        // Progress observers cannot alter comparison evidence or verdicts.
    }
}

void appendTiming(reporting::Report& report,
                  std::string phase,
                  Clock::time_point started) {
    const auto elapsed = std::chrono::duration<double, std::milli>(
        Clock::now() - started);
    report.timings.push_back({std::move(phase), elapsed.count()});
}

ComparisonResult cancelledResult(const ComparisonRequest& request,
                                 reporting::Report report = {}) {
    ComparisonResult result;
    if (report.inputA.pathUtf8.empty() && report.inputB.pathUtf8.empty()) {
        initializeReport(report, request);
    }
    result.report = std::move(report);
    result.status = ComparisonRunStatus::Cancelled;
    result.verdict =
        {domain::Decision::Check, {domain::ReasonCode::EvidenceIncomplete}};
    result.report.verdict.decision = "CHECK";
    result.report.verdict.reasons = {"CANCELLED"};
    result.report.execution.status = "CANCELLED";
    result.report.execution.cancellationRequested = true;
    result.report.execution.allRequiredEvidenceComplete = false;
    result.diagnostics.push_back(
        {ComparisonDiagnosticCode::Cancelled, "Comparison was cancelled"});
    return result;
}

void appendImportDiagnostics(ComparisonResult& result,
                             const import::StepImportResult& imported,
                             ComparisonDiagnosticCode code,
                             std::string_view fallback) {
    if (imported.diagnostics.empty()) {
        result.diagnostics.push_back({code, std::string(fallback)});
        return;
    }
    for (const auto& diagnosticValue : imported.diagnostics) {
        result.diagnostics.push_back({code, diagnosticValue.messageUtf8});
    }
}

ComponentMatchRow singlePartRow(const AssemblyIndex& indexA,
                                const AssemblyIndex& indexB) {
    ComponentMatchRow row;
    row.nodeIdA = indexA.occurrences.front().nodeId;
    row.nodeIdB = indexB.occurrences.front().nodeId;
    row.matchStatus = MatchStatus::MatchGeometry;
    return row;
}

void runSinglePart(const ComparisonRequest& request,
                   const AssemblyIndex& indexA,
                   const AssemblyIndex& indexB,
                   deep::DeepGeometryPort& deepGeometry,
                   deviation::SurfaceDeviationPort* surfaceDeviation,
                   domain::EvidenceSummary& evidence,
                   reporting::Report& report,
                   std::unordered_map<std::string, deep::DeepGeometryResult>&
                       deepResults,
                   bool& deepFailed,
                   bool& deviationFailed,
                   bool& surfaceCancelled) {
    const auto& prototypeA = indexA.prototypes.front();
    const auto& prototypeB = indexB.prototypes.front();
    ComponentMatchRow row = singlePartRow(indexA, indexB);
    const auto fast = domain::compareFastInvariants(
        prototypeA.statistics, prototypeB.statistics, request.tolerances);
    reporting::DeviationStatistics componentDeviation;

    std::optional<deep::DeepGeometryResult> deepResult;
    if (fast.status == domain::FastScreenStatus::Different) {
        evidence.geometry = domain::GeometryStatus::ChangedProven;
        row.geometryEvidence = GeometryEvidence::DifferentProven;
        row.resultStatus = ComponentResultStatus::GeometryChanged;
        row.confidence = 1.0;
    } else if (fast.status == domain::FastScreenStatus::CompatibleCandidate &&
               request.deep) {
        deepResult = deepGeometry.compareAligned({
            prototypeA.geometry,
            prototypeB.geometry,
            {request.tolerances.booleanFuzzyMm,
             request.tolerances.relativeProperty},
        });
        deepResults.insert_or_assign(
            prototypeA.id + '\x1f' + prototypeB.id, *deepResult);
        switch (deepResult->status) {
        case deep::DeepGeometryStatus::SameGeometry:
            evidence.geometry = domain::GeometryStatus::SameProven;
            row.geometryEvidence = GeometryEvidence::SameProven;
            row.resultStatus = ComponentResultStatus::Same;
            row.confidence = 1.0;
            break;
        case deep::DeepGeometryStatus::GeometryChanged:
            evidence.geometry = domain::GeometryStatus::ChangedProven;
            row.geometryEvidence = GeometryEvidence::DifferentProven;
            row.resultStatus = ComponentResultStatus::GeometryChanged;
            row.confidence = 1.0;
            break;
        case deep::DeepGeometryStatus::AlignmentNotProven:
            evidence.alignmentAmbiguous = true;
            row.resultStatus = ComponentResultStatus::Ambiguous;
            break;
        case deep::DeepGeometryStatus::OpenShellUnsupported:
            row.resultStatus = ComponentResultStatus::Check;
            break;
        case deep::DeepGeometryStatus::Error:
            deepFailed = true;
            row.resultStatus = ComponentResultStatus::Check;
            break;
        }
    } else {
        row.resultStatus = ComponentResultStatus::Check;
    }

    const domain::PlacementInput placementInput{
        prototypeA.statistics,
        prototypeB.statistics,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
    };
    const auto placement =
        domain::analyzeAbsolutePlacement(placementInput, request.tolerances);
    const auto& occurrenceA = indexA.occurrences.front();
    const auto& occurrenceB = indexB.occurrences.front();
    const domain::Vec3Mm occurrenceDelta{
        occurrenceB.worldTransform.matrix[3] -
            occurrenceA.worldTransform.matrix[3],
        occurrenceB.worldTransform.matrix[7] -
            occurrenceA.worldTransform.matrix[7],
        occurrenceB.worldTransform.matrix[11] -
            occurrenceA.worldTransform.matrix[11],
    };
    const double occurrenceDistance = std::sqrt(
        occurrenceDelta.x * occurrenceDelta.x +
        occurrenceDelta.y * occurrenceDelta.y +
        occurrenceDelta.z * occurrenceDelta.z);
    const bool explicitOccurrenceMove =
        occurrenceDistance > request.tolerances.positionMm;
    const domain::Vec3Mm reportedDelta =
        explicitOccurrenceMove ? occurrenceDelta : placement.deltaBMinusA;
    report.placement.translationBMinusAMm = reportVector(reportedDelta);
    row.translationBMinusAMm = reportedDelta;

    bool placementKnown =
        placement.status == domain::PlacementAnalysisStatus::Same ||
        placement.status == domain::PlacementAnalysisStatus::Translated;
    bool moved = explicitOccurrenceMove ||
                 placement.status ==
                     domain::PlacementAnalysisStatus::Translated;

    const double occurrenceRotation = relativeRotationAngleDegrees(
        occurrenceA.worldTransform, occurrenceB.worldTransform);
    bool rotated =
        occurrenceRotation > request.tolerances.angularDegrees;
    row.rotationAngleDegrees = occurrenceRotation;
    report.placement.rotationAngleDegrees = occurrenceRotation;
    if (deepResult && deepResult->alignmentProven) {
        const double angle = rotationAngleDegrees(deepResult->transformBToA);
        if (!rotated) {
            rotated = angle > request.tolerances.angularDegrees;
            row.rotationAngleDegrees = angle;
            report.placement.rotationAngleDegrees = angle;
        }
        report.placement.rotationBToA =
            quaternionFromTransform(deepResult->transformBToA);
    }

    if (request.deep && deepResult && deepResult->alignmentProven) {
        const auto deviationResult = runDeviation(
            request,
            surfaceDeviation,
            prototypeA.geometry,
            prototypeB.geometry,
            deepResult->transformBToA);
        if (!deviationResult) {
            deviationFailed = true;
        } else {
            componentDeviation = reportDeviation(*deviationResult);
            report.deepDeviation = componentDeviation;
            switch (deviationResult->status) {
            case deviation::SurfaceDeviationStatus::WithinTolerance:
                break;
            case deviation::SurfaceDeviationStatus::DeviationFound:
                evidence.geometry = domain::GeometryStatus::ChangedProven;
                row.geometryEvidence = GeometryEvidence::DifferentProven;
                row.resultStatus = ComponentResultStatus::GeometryChanged;
                break;
            case deviation::SurfaceDeviationStatus::Cancelled:
                surfaceCancelled = true;
                break;
            case deviation::SurfaceDeviationStatus::NoSurfaceData:
            case deviation::SurfaceDeviationStatus::Error:
                deviationFailed = true;
                row.resultStatus = ComponentResultStatus::Check;
                break;
            }
        }
    } else if (request.deep &&
               evidence.geometry == domain::GeometryStatus::SameProven) {
        // A canonical deep PASS requires quantitative surface evidence.
        deviationFailed = true;
        row.resultStatus = ComponentResultStatus::Check;
    }
    if (explicitOccurrenceMove || rotated) {
        placementKnown = true;
    }
    if (placementKnown) {
        evidence.position = positionStatus(moved, rotated);
    } else {
        evidence.position = domain::PositionStatus::Unknown;
    }
    if (row.resultStatus == ComponentResultStatus::Same) {
        row.resultStatus = moved && rotated
                               ? ComponentResultStatus::MovedAndRotated
                               : moved ? ComponentResultStatus::Moved
                               : rotated ? ComponentResultStatus::Rotated
                                         : ComponentResultStatus::Same;
    }
    evidence.allRequiredStagesComplete =
        evidence.geometry == domain::GeometryStatus::SameProven &&
        evidence.position != domain::PositionStatus::Unknown &&
        !evidence.alignmentAmbiguous && !deepFailed && !deviationFailed &&
        !surfaceCancelled && report.deepDeviation.available;
    auto componentReport = reportComponent(row, indexA, indexB);
    componentReport.deviation = componentDeviation;
    report.components.push_back(std::move(componentReport));
}

void runAssembly(const ComparisonRequest& request,
                 const AssemblyIndex& indexA,
                 const AssemblyIndex& indexB,
                 deep::DeepGeometryPort& deepGeometry,
                 deviation::SurfaceDeviationPort* surfaceDeviation,
                 domain::EvidenceSummary& evidence,
                 reporting::Report& report,
                 std::unordered_map<std::string, deep::DeepGeometryResult>&
                     deepResults,
                 bool& deepFailed,
                 bool& deviationFailed,
                 bool& surfaceCancelled) {
    bool alignmentAmbiguous = false;
    std::size_t completedDeepChecks = 0U;
    const std::size_t expectedDeepChecks =
        std::max({indexA.prototypes.size(),
                  indexB.prototypes.size(),
                  std::size_t{1}});
    assembly::MatchingOptions options;
    options.tolerances = request.tolerances;
    // XCAF label paths are trusted here only as occurrence-correspondence
    // evidence. matchComponents also requires compatible occurrence names and
    // still performs fast/deep B-Rep verification, so identity can never turn
    // changed or inconclusive geometry into SAME. This avoids the quadratic
    // cross-prototype search and prevents a changed part at the same assembly
    // path from being emitted as a misleading MISSING + NEW pair.
    options.stableNodeIdsTrusted = true;
    if (request.deep) {
        options.deepVerifier =
            [&](const assembly::IndexedPrototype& prototypeA,
                const assembly::IndexedPrototype& prototypeB) {
                const auto verified = deepGeometry.compareAligned({
                    prototypeA.geometry,
                    prototypeB.geometry,
                    {request.tolerances.booleanFuzzyMm,
                     request.tolerances.relativeProperty},
                });
                deepResults.insert_or_assign(
                    prototypeA.id + '\x1f' + prototypeB.id, verified);
                ++completedDeepChecks;
                notifyProgress(
                    request,
                    ComparisonPhase::Matching,
                    std::size_t{60} + std::min(
                              std::size_t{19},
                              (completedDeepChecks * 20U) /
                                  expectedDeepChecks),
                    100U);
                switch (verified.status) {
                case deep::DeepGeometryStatus::SameGeometry:
                    return assembly::DeepVerification{
                        GeometryEvidence::SameProven, 1.0};
                case deep::DeepGeometryStatus::GeometryChanged:
                    return assembly::DeepVerification{
                        GeometryEvidence::DifferentProven, 1.0};
                case deep::DeepGeometryStatus::AlignmentNotProven:
                    alignmentAmbiguous = true;
                    return assembly::DeepVerification{};
                case deep::DeepGeometryStatus::OpenShellUnsupported:
                    return assembly::DeepVerification{};
                case deep::DeepGeometryStatus::Error:
                    deepFailed = true;
                    return assembly::DeepVerification{};
                }
                return assembly::DeepVerification{};
            };
    }

    const AssemblyMatchResult matching =
        assembly::matchComponents(indexA, indexB, options);
    bool allGeometrySame = !matching.rows.empty();
    bool anyGeometryChanged = false;
    bool moved = false;
    bool rotated = false;
    double deviationMeanWeighted{};
    double deviationRmsSquaredWeighted{};
    std::uint64_t deviationSamples{};
    std::uint64_t deviationEvaluations{};
    double maximumDeviation{};
    double maximumPercentile95{};
    // Surface deviation belongs to an aligned prototype pair. Repeated
    // assembly occurrences must reuse that evidence instead of remeshing and
    // resampling identical prototypes thousands of times. The cached result is
    // still accumulated once per occurrence below, preserving report
    // aggregation semantics while removing duplicate OCCT work.
    std::unordered_map<
        std::string,
        std::optional<deviation::SurfaceDeviationResult>> deviationResults;
    for (const auto& row : matching.rows) {
        evidence.componentMissing |=
            row.resultStatus == ComponentResultStatus::Missing;
        evidence.componentAdded |=
            row.resultStatus == ComponentResultStatus::New;
        anyGeometryChanged |=
            row.resultStatus == ComponentResultStatus::GeometryChanged;
        allGeometrySame &=
            row.geometryEvidence == GeometryEvidence::SameProven;
        moved |= row.resultStatus == ComponentResultStatus::Moved ||
                 row.resultStatus == ComponentResultStatus::MovedAndRotated;
        rotated |= row.resultStatus == ComponentResultStatus::Rotated ||
                   row.resultStatus == ComponentResultStatus::MovedAndRotated;
        auto componentReport = reportComponent(row, indexA, indexB);
        const auto* prototypeA = prototypeFor(indexA, row.nodeIdA);
        const auto* prototypeB = prototypeFor(indexB, row.nodeIdB);
        if (request.deep && prototypeA != nullptr && prototypeB != nullptr &&
            row.geometryEvidence == GeometryEvidence::SameProven) {
            import::RigidTransformMm transformBToA;
            const auto foundDeep = deepResults.find(
                prototypeA->id + '\x1f' + prototypeB->id);
            if (foundDeep != deepResults.end() &&
                foundDeep->second.alignmentProven) {
                transformBToA = foundDeep->second.transformBToA;
            }
            const std::string deviationKey =
                prototypeA->id + '\x1f' + prototypeB->id;
            auto [cachedDeviation, insertedDeviation] =
                deviationResults.try_emplace(deviationKey);
            if (insertedDeviation) {
                cachedDeviation->second = runDeviation(
                    request,
                    surfaceDeviation,
                    prototypeA->geometry,
                    prototypeB->geometry,
                    transformBToA);
            }
            const auto& deviationResult = cachedDeviation->second;
            if (!deviationResult) {
                deviationFailed = true;
                componentReport.geometryStatus = "INCONCLUSIVE";
                componentReport.positionStatus = "UNKNOWN";
            } else {
                componentReport.deviation = reportDeviation(*deviationResult);
                if (componentReport.deviation.available) {
                    const auto samples = componentReport.deviation.sampleCount;
                    deviationSamples += samples;
                    deviationMeanWeighted +=
                        deviationResult->meanMm * static_cast<double>(samples);
                    deviationRmsSquaredWeighted +=
                        deviationResult->rmsMm * deviationResult->rmsMm *
                        static_cast<double>(samples);
                    deviationEvaluations += static_cast<std::uint64_t>(
                        deviationResult->triangleDistanceEvaluations);
                    maximumDeviation =
                        std::max(maximumDeviation, deviationResult->maximumMm);
                    maximumPercentile95 = std::max(
                        maximumPercentile95, deviationResult->percentileMm);
                }
                switch (deviationResult->status) {
                case deviation::SurfaceDeviationStatus::WithinTolerance:
                    break;
                case deviation::SurfaceDeviationStatus::DeviationFound:
                    anyGeometryChanged = true;
                    allGeometrySame = false;
                    componentReport.geometryStatus = "DIFFERENT_PROVEN";
                    break;
                case deviation::SurfaceDeviationStatus::Cancelled:
                    surfaceCancelled = true;
                    break;
                case deviation::SurfaceDeviationStatus::NoSurfaceData:
                case deviation::SurfaceDeviationStatus::Error:
                    deviationFailed = true;
                    componentReport.geometryStatus = "INCONCLUSIVE";
                    break;
                }
            }
        }
        report.components.push_back(std::move(componentReport));
    }
    if (deviationSamples > 0U) {
        report.deepDeviation.available = true;
        report.deepDeviation.maximumMm = maximumDeviation;
        report.deepDeviation.meanMm =
            deviationMeanWeighted / static_cast<double>(deviationSamples);
        report.deepDeviation.rmsMm = std::sqrt(
            deviationRmsSquaredWeighted / static_cast<double>(deviationSamples));
        report.deepDeviation.percentile95Mm = maximumPercentile95;
        report.deepDeviation.sampleCount = deviationSamples;
        report.deepDeviation.triangleDistanceEvaluations = deviationEvaluations;
    }
    evidence.geometry = anyGeometryChanged
                            ? domain::GeometryStatus::ChangedProven
                            : allGeometrySame
                                  ? domain::GeometryStatus::SameProven
                                  : domain::GeometryStatus::Unknown;
    evidence.position = positionStatus(moved, rotated);
    evidence.alignmentAmbiguous = alignmentAmbiguous;
    evidence.allRequiredStagesComplete =
        matching.completeWithoutAmbiguity && allGeometrySame && !deepFailed &&
        !deviationFailed && !surfaceCancelled && report.deepDeviation.available;
}

bool inputsAreByteIdentical(const ComparisonRequest& request) noexcept {
    return request.identityA && request.identityB &&
           !request.identityA->sha256Hex.empty() &&
           request.identityA->sha256Hex == request.identityB->sha256Hex &&
           request.identityA->sizeBytes == request.identityB->sizeBytes;
}

bool runByteIdenticalProof(const AssemblyIndex& indexA,
                           const AssemblyIndex& indexB,
                           domain::EvidenceSummary& evidence,
                           reporting::Report& report) {
    if (indexA.occurrences.size() != indexB.occurrences.size()) {
        return false;
    }
    std::vector<reporting::ComponentRow> rows;
    rows.reserve(indexA.occurrences.size());
    for (const auto& occurrenceA : indexA.occurrences) {
        const auto* occurrenceB = indexB.findOccurrence(occurrenceA.nodeId);
        if (occurrenceB == nullptr ||
            occurrenceA.worldTransform.matrix !=
                occurrenceB->worldTransform.matrix ||
            indexA.prototypes[occurrenceA.prototypeIndex].id !=
                indexB.prototypes[occurrenceB->prototypeIndex].id) {
            return false;
        }
        ComponentMatchRow row;
        row.nodeIdA = occurrenceA.nodeId;
        row.nodeIdB = occurrenceB->nodeId;
        row.matchStatus = MatchStatus::MatchExact;
        row.geometryEvidence = GeometryEvidence::SameProven;
        row.resultStatus = ComponentResultStatus::Same;
        row.confidence = 1.0;
        auto component = reportComponent(row, indexA, indexB);
        component.deviation.available = true;
        rows.push_back(std::move(component));
    }
    report.components = std::move(rows);
    // Equal SHA-256 and byte size under the same import configuration prove
    // that A and B are the same source stream. Zero deviation is derived from
    // that exact identity proof; no surface samples are claimed.
    report.deepDeviation.available = true;
    report.deepDeviation.maximumMm = 0.0;
    report.deepDeviation.meanMm = 0.0;
    report.deepDeviation.rmsMm = 0.0;
    report.deepDeviation.percentile95Mm = 0.0;
    report.deepDeviation.sampleCount = 0U;
    report.deepDeviation.triangleDistanceEvaluations = 0U;
    evidence.geometry = domain::GeometryStatus::SameProven;
    evidence.position = domain::PositionStatus::Same;
    evidence.allRequiredStagesComplete = true;
    return true;
}

ComparisonResult compareImpl(const ComparisonRequest& request,
                             import::StepImportPort& importer,
                             deep::DeepGeometryPort& deepGeometry,
                             deviation::SurfaceDeviationPort* surfaceDeviation,
                             feature::FeatureRecognitionPort* featureRecognition) {
    ComparisonResult result;
    initializeReport(result.report, request);

    if (request.cancellation.stop_requested()) {
        return cancelledResult(request, std::move(result.report));
    }

    notifyProgress(request, ComparisonPhase::ImportA, 0U);
    result.report.execution.terminalPhase = "IMPORT_A";
    const auto importAStarted = Clock::now();
    const auto importedA =
        importer.importStep(import::StepImportRequest{request.inputAUtf8});
    appendTiming(result.report, "import_a", importAStarted);
    if (request.cancellation.stop_requested()) {
        return cancelledResult(request, std::move(result.report));
    }
    notifyProgress(request, ComparisonPhase::ImportB, 1U);
    result.report.execution.terminalPhase = "IMPORT_B";
    const auto importBStarted = Clock::now();
    const auto importedB =
        importer.importStep(import::StepImportRequest{request.inputBUtf8});
    appendTiming(result.report, "import_b", importBStarted);
    if (request.cancellation.stop_requested()) {
        return cancelledResult(request, std::move(result.report));
    }
    if (!importedA.succeeded() || !importedB.succeeded()) {
        result.status = ComparisonRunStatus::InputError;
        if (!importedA.succeeded()) {
            appendImportDiagnostics(result,
                                    importedA,
                                    ComparisonDiagnosticCode::ImportAFailed,
                                    "STEP import A failed");
        }
        if (!importedB.succeeded()) {
            appendImportDiagnostics(result,
                                    importedB,
                                    ComparisonDiagnosticCode::ImportBFailed,
                                    "STEP import B failed");
        }
        domain::EvidenceSummary evidence;
        evidence.stepImportFailed = true;
        result.verdict = domain::reduceVerdict(evidence);
        applyVerdict(result.report, result.verdict);
        result.report.execution.status = "INPUT_ERROR";
        result.report.execution.allRequiredEvidenceComplete = false;
        return result;
    }

    notifyProgress(request, ComparisonPhase::AssemblyIndex, 2U);
    result.report.execution.terminalPhase = "ASSEMBLY_INDEX";
    const auto indexingStarted = Clock::now();
    const auto indexedA = assembly::buildAssemblyIndex(importedA.model);
    const auto indexedB = assembly::buildAssemblyIndex(importedB.model);
    appendTiming(result.report, "assembly_index", indexingStarted);
    if (request.cancellation.stop_requested()) {
        return cancelledResult(request, std::move(result.report));
    }
    if (!indexedA || !indexedB) {
        result.status = ComparisonRunStatus::ProcessingError;
        if (!indexedA) {
            result.diagnostics.push_back({
                ComparisonDiagnosticCode::AssemblyIndexAFailed,
                "Imported model A failed assembly-index validation"});
        }
        if (!indexedB) {
            result.diagnostics.push_back({
                ComparisonDiagnosticCode::AssemblyIndexBFailed,
                "Imported model B failed assembly-index validation"});
        }
        result.verdict =
            {domain::Decision::Error, {domain::ReasonCode::EvidenceIncomplete}};
        result.report.verdict.decision = "ERROR";
        result.report.verdict.reasons = {"ASSEMBLY_INDEX_FAILED"};
        result.report.execution.status = "PROCESSING_ERROR";
        result.report.execution.allRequiredEvidenceComplete = false;
        return result;
    }

    result.report.statisticsA =
        reportStatistics(aggregateStatistics(*indexedA.index));
    result.report.statisticsB =
        reportStatistics(aggregateStatistics(*indexedB.index));

    domain::EvidenceSummary evidence;
    bool deepFailed = false;
    bool deviationFailed = false;
    bool surfaceCancelled = false;
    std::unordered_map<std::string, deep::DeepGeometryResult> deepResults;
    notifyProgress(request, ComparisonPhase::Matching, 3U);
    result.report.execution.terminalPhase = "MATCHING_AND_DEEP_EVIDENCE";
    const auto matchingStarted = Clock::now();
    const bool singlePart = importedA.model.nodes.size() == 1U &&
                            importedB.model.nodes.size() == 1U &&
                            !importedA.model.nodes.front().isAssembly &&
                            !importedB.model.nodes.front().isAssembly &&
                            indexedA.index->occurrences.size() == 1U &&
                            indexedB.index->occurrences.size() == 1U &&
                            indexedA.index->prototypes.size() == 1U &&
                            indexedB.index->prototypes.size() == 1U;
    const bool exactIdentityProven =
        inputsAreByteIdentical(request) &&
        runByteIdenticalProof(*indexedA.index,
                              *indexedB.index,
                              evidence,
                              result.report);
    if (exactIdentityProven) {
        result.report.execution.terminalPhase = "BYTE_IDENTICAL_PROOF";
    } else if (singlePart) {
        runSinglePart(request,
                      *indexedA.index,
                      *indexedB.index,
                      deepGeometry,
                      surfaceDeviation,
                      evidence,
                      result.report,
                      deepResults,
                      deepFailed,
                      deviationFailed,
                      surfaceCancelled);
    } else {
        runAssembly(request,
                    *indexedA.index,
                    *indexedB.index,
                    deepGeometry,
                    surfaceDeviation,
                    evidence,
                    result.report,
                    deepResults,
                    deepFailed,
                    deviationFailed,
                    surfaceCancelled);
    }
    appendTiming(result.report, "comparison", matchingStarted);
    if (request.cancellation.stop_requested() || surfaceCancelled) {
        return cancelledResult(request, std::move(result.report));
    }

    const bool featureEvidenceEnabled =
        request.featureEvidenceScope == FeatureEvidenceScope::AllMatchedComponents ||
        singlePart;
    if (featureRecognition != nullptr && request.deep && featureEvidenceEnabled) {
        if (request.cancellation.stop_requested()) {
            return cancelledResult(request, std::move(result.report));
        }
        const auto featureStarted = Clock::now();
        notifyProgress(request, ComparisonPhase::FeatureEvidence, 4U);
        const auto featureStatus = appendFeatureEvidence(*indexedA.index,
                                                         *indexedB.index,
                                                         request.tolerances,
                                                         deepGeometry,
                                                         *featureRecognition,
                                                         exactIdentityProven,
                                                         deepResults,
                                                         request.cancellation,
                                                         result.report);
        appendTiming(result.report, "feature_evidence", featureStarted);
        if (featureStatus == FeatureEvidenceStatus::Cancelled ||
            request.cancellation.stop_requested()) {
            return cancelledResult(request, std::move(result.report));
        }
    }

    if (request.cancellation.stop_requested()) {
        return cancelledResult(request, std::move(result.report));
    }
    evidence.deepCheckFailed = deepFailed || deviationFailed;
    result.verdict = domain::reduceVerdict(evidence);
    result.status = (deepFailed || deviationFailed)
                        ? ComparisonRunStatus::ProcessingError
                        : ComparisonRunStatus::Completed;
    if (deepFailed) {
        result.diagnostics.push_back({
            ComparisonDiagnosticCode::DeepComparisonFailed,
            "Deep geometry adapter failed; comparison cannot be completed"});
    }
    if (deviationFailed) {
        result.diagnostics.push_back({
            ComparisonDiagnosticCode::SurfaceDeviationFailed,
            "Surface deviation evidence was unavailable or failed; PASS is forbidden"});
    }
    if (request.cancellation.stop_requested()) {
        return cancelledResult(request, std::move(result.report));
    }
    applyVerdict(result.report, result.verdict);
    result.report.execution.status =
        result.status == ComparisonRunStatus::Completed ? "COMPLETED"
                                                        : "PROCESSING_ERROR";
    result.report.execution.terminalPhase = "COMPLETE";
    result.report.execution.allRequiredEvidenceComplete =
        evidence.allRequiredStagesComplete &&
        result.status == ComparisonRunStatus::Completed;
    notifyProgress(request, ComparisonPhase::Complete, 5U);
    return result;
}

void appendKeyDouble(std::string& output, const double value) {
    std::array<char, 64> buffer{};
    const auto converted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value,
        std::chars_format::general, std::numeric_limits<double>::max_digits10);
    if (converted.ec != std::errc{}) {
        throw std::runtime_error("Unable to serialize comparison cache key");
    }
    output.append(buffer.data(), converted.ptr);
}

std::optional<std::string> comparisonCacheKey(
    const ComparisonRequest& request) {
    if (!request.enableCache || !request.identityA || !request.identityB) {
        return std::nullopt;
    }
    std::string configuration = request.importConfiguration;
    configuration += request.deep ? "|deep=1" : "|deep=0";
    configuration += request.featureEvidenceScope ==
                             FeatureEvidenceScope::AllMatchedComponents
                         ? "|featureScope=all"
                         : "|featureScope=single-part";
    configuration += "|position=";
    appendKeyDouble(configuration, request.tolerances.positionMm);
    configuration += "|surface=";
    appendKeyDouble(configuration, request.tolerances.surfaceMm);
    configuration += "|angle=";
    appendKeyDouble(configuration, request.tolerances.angularDegrees);
    configuration += "|boolean=";
    appendKeyDouble(configuration, request.tolerances.booleanFuzzyMm);
    configuration += "|relative=";
    appendKeyDouble(configuration, request.tolerances.relativeProperty);

    const auto keyA = cache::makeCacheKey(
        {*request.identityA, configuration, "dev-v1-comparison-v2"});
    const auto keyB = cache::makeCacheKey(
        {*request.identityB, configuration, "dev-v1-comparison-v2"});
    return "A{" + keyA + "}|B{" + keyB + "}";
}

std::size_t estimatedBytes(const ComparisonResult& result) noexcept {
    std::size_t bytes = sizeof(result);
    bytes += result.report.components.size() * sizeof(reporting::ComponentRow);
    bytes += result.report.timings.size() * sizeof(reporting::Timing);
    bytes += result.report.features.size() * sizeof(reporting::FeatureRow);
    for (const auto& component : result.report.components) {
        bytes += component.idA.size() + component.idB.size() +
                 component.nameA.size() + component.nameB.size() +
                 component.matchStatus.size() + component.geometryStatus.size() +
                 component.positionStatus.size();
    }
    for (const auto& diagnostic : result.diagnostics) {
        bytes += diagnostic.messageUtf8.size();
    }
    for (const auto& feature : result.report.features) {
        bytes += feature.idA.size() + feature.idB.size() +
                 feature.ownerComponentIdA.size() +
                 feature.ownerComponentIdB.size() + feature.type.size() +
                 feature.evidenceStatus.size() + feature.result.size() +
                 feature.reason.size() + feature.profileA.size() +
                 feature.profileB.size() +
                 (feature.faceIndicesA.size() + feature.faceIndicesB.size()) *
                     sizeof(std::uint32_t);
    }
    return bytes;
}

void applyCacheMetadata(reporting::Report& report,
                        const std::optional<std::string>& key,
                        const cache::CacheStatistics& statistics,
                        const std::size_t usedBytes,
                        const std::size_t budgetBytes,
                        const bool hit) {
    report.cache.enabled = key.has_value();
    report.cache.hit = hit;
    report.cache.key = key.value_or("");
    report.cache.hits = statistics.hits;
    report.cache.misses = statistics.misses;
    report.cache.evictions = statistics.evictions;
    report.cache.usedBytes = static_cast<std::uint64_t>(usedBytes);
    report.cache.budgetBytes = static_cast<std::uint64_t>(budgetBytes);
}

}  // namespace

ComparisonCoordinator::ComparisonCoordinator(
    import::StepImportPort& importer,
    deep::DeepGeometryPort& deepGeometry,
    deviation::SurfaceDeviationPort* surfaceDeviation,
    const std::size_t cacheBudgetBytes) noexcept
    : importer_(importer),
      deepGeometry_(deepGeometry),
      surfaceDeviation_(surfaceDeviation),
      cache_(cacheBudgetBytes) {}

ComparisonCoordinator::ComparisonCoordinator(
    import::StepImportPort& importer,
    deep::DeepGeometryPort& deepGeometry,
    deviation::SurfaceDeviationPort* surfaceDeviation,
    feature::FeatureRecognitionPort* featureRecognition,
    const std::size_t cacheBudgetBytes) noexcept
    : importer_(importer),
      deepGeometry_(deepGeometry),
      surfaceDeviation_(surfaceDeviation),
      featureRecognition_(featureRecognition),
      cache_(cacheBudgetBytes) {}

ComparisonResult ComparisonCoordinator::compare(
    const ComparisonRequest& request) noexcept {
    try {
        if (request.cancellation.stop_requested()) {
            return cancelledResult(request);
        }
        const auto key = comparisonCacheKey(request);
        if (key) {
            const auto lookupStarted = Clock::now();
            if (auto cached = cache_.get(*key)) {
                if (request.cancellation.stop_requested()) {
                    return cancelledResult(request);
                }
                cached->report.inputA.pathUtf8 = utf8Bytes(request.inputAUtf8);
                cached->report.inputB.pathUtf8 = utf8Bytes(request.inputBUtf8);
                appendTiming(cached->report, "cache_lookup", lookupStarted);
                applyCacheMetadata(cached->report,
                                   key,
                                   cache_.statistics(),
                                   cache_.usedBytes(),
                                   cache_.budgetBytes(),
                                   true);
                return *cached;
            }
        }

        auto result =
            compareImpl(request,
                        importer_,
                        deepGeometry_,
                        surfaceDeviation_,
                        featureRecognition_);
        if (key && result.status == ComparisonRunStatus::Completed &&
            !request.cancellation.stop_requested()) {
            static_cast<void>(cache_.put(*key, result, estimatedBytes(result)));
        }
        applyCacheMetadata(result.report,
                           key,
                           cache_.statistics(),
                           cache_.usedBytes(),
                           cache_.budgetBytes(),
                           false);
        return result;
    } catch (const std::exception& failure) {
        ComparisonResult result;
        initializeReport(result.report, request);
        result.status = ComparisonRunStatus::ProcessingError;
        result.verdict =
            {domain::Decision::Error, {domain::ReasonCode::EvidenceIncomplete}};
        result.report.verdict.decision = "ERROR";
        result.report.verdict.reasons = {"INTERNAL_FAILURE"};
        result.report.execution.status = "PROCESSING_ERROR";
        result.report.execution.terminalPhase = "INTERNAL_FAILURE";
        result.diagnostics.push_back(
            {ComparisonDiagnosticCode::InternalFailure, failure.what()});
        return result;
    } catch (...) {
        ComparisonResult result;
        initializeReport(result.report, request);
        result.status = ComparisonRunStatus::ProcessingError;
        result.verdict =
            {domain::Decision::Error, {domain::ReasonCode::EvidenceIncomplete}};
        result.report.verdict.decision = "ERROR";
        result.report.verdict.reasons = {"INTERNAL_FAILURE"};
        result.report.execution.status = "PROCESSING_ERROR";
        result.report.execution.terminalPhase = "INTERNAL_FAILURE";
        result.diagnostics.push_back({
            ComparisonDiagnosticCode::InternalFailure,
            "Unknown application coordinator failure"});
        return result;
    }
}

FeaturePairComparisonResult ComparisonCoordinator::compareFeaturePair(
    const FeaturePairComparisonRequest& request) noexcept {
    FeaturePairComparisonResult result;
    result.componentIdA = request.componentIdA;
    result.componentIdB = request.componentIdB;
    const auto cancelled = [&result] {
        result.status = ComparisonRunStatus::Cancelled;
        result.diagnostics.push_back(
            {ComparisonDiagnosticCode::Cancelled,
             "Feature pair comparison was cancelled"});
        return result;
    };
    try {
        if (request.cancellation.stop_requested()) {
            return cancelled();
        }
        if (featureRecognition_ == nullptr || request.componentIdA.empty() ||
            request.componentIdB.empty()) {
            result.status = ComparisonRunStatus::ProcessingError;
            result.diagnostics.push_back(
                {ComparisonDiagnosticCode::InternalFailure,
                 "Feature pair comparison requires a recognizer and two occurrence IDs"});
            return result;
        }

        const auto importedA = importer_.importStep({request.inputAUtf8});
        if (request.cancellation.stop_requested()) {
            return cancelled();
        }
        const auto importedB = importer_.importStep({request.inputBUtf8});
        if (request.cancellation.stop_requested()) {
            return cancelled();
        }
        if (!importedA.succeeded() || !importedB.succeeded()) {
            result.status = ComparisonRunStatus::InputError;
            if (!importedA.succeeded()) {
                result.diagnostics.push_back(
                    {ComparisonDiagnosticCode::ImportAFailed,
                     "STEP import A failed during feature pair comparison"});
            }
            if (!importedB.succeeded()) {
                result.diagnostics.push_back(
                    {ComparisonDiagnosticCode::ImportBFailed,
                     "STEP import B failed during feature pair comparison"});
            }
            return result;
        }

        const auto indexedA = assembly::buildAssemblyIndex(importedA.model);
        const auto indexedB = assembly::buildAssemblyIndex(importedB.model);
        if (request.cancellation.stop_requested()) {
            return cancelled();
        }
        if (!indexedA || !indexedB) {
            result.status = ComparisonRunStatus::ProcessingError;
            if (!indexedA) {
                result.diagnostics.push_back(
                    {ComparisonDiagnosticCode::AssemblyIndexAFailed,
                     "Assembly index A failed during feature pair comparison"});
            }
            if (!indexedB) {
                result.diagnostics.push_back(
                    {ComparisonDiagnosticCode::AssemblyIndexBFailed,
                     "Assembly index B failed during feature pair comparison"});
            }
            return result;
        }
        const auto* occurrenceA =
            indexedA.index->findOccurrence(request.componentIdA);
        const auto* occurrenceB =
            indexedB.index->findOccurrence(request.componentIdB);
        if (occurrenceA == nullptr || occurrenceB == nullptr) {
            result.status = ComparisonRunStatus::ProcessingError;
            result.diagnostics.push_back(
                {ComparisonDiagnosticCode::InternalFailure,
                 "Selected feature pair occurrence was not found after import"});
            return result;
        }

        reporting::Report pairReport;
        pairReport.components.push_back({
            .idA = request.componentIdA,
            .idB = request.componentIdB,
            .nameA = occurrenceA->nameUtf8,
            .nameB = occurrenceB->nameUtf8,
        });
        const std::unordered_map<std::string, deep::DeepGeometryResult>
            noPrecomputedAlignments;
        const auto evidenceStatus = appendFeatureEvidence(
            *indexedA.index,
            *indexedB.index,
            request.tolerances,
            deepGeometry_,
            *featureRecognition_,
            false,
            noPrecomputedAlignments,
            request.cancellation,
            pairReport);
        if (evidenceStatus == FeatureEvidenceStatus::Cancelled ||
            request.cancellation.stop_requested()) {
            return cancelled();
        }
        result.features = std::move(pairReport.features);
        result.status = ComparisonRunStatus::Completed;
        return result;
    } catch (const std::exception& failure) {
        result.status = ComparisonRunStatus::ProcessingError;
        result.diagnostics.push_back(
            {ComparisonDiagnosticCode::InternalFailure, failure.what()});
        return result;
    } catch (...) {
        result.status = ComparisonRunStatus::ProcessingError;
        result.diagnostics.push_back(
            {ComparisonDiagnosticCode::InternalFailure,
             "Unknown feature pair comparison failure"});
        return result;
    }
}

}  // namespace stepcompare::application
