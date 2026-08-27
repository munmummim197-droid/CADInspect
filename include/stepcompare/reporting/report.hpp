#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace stepcompare::reporting {

struct Vector3 final {
    double x{};
    double y{};
    double z{};
};

struct Quaternion final {
    double w{1.0};
    double x{};
    double y{};
    double z{};
};

struct InputIdentity final {
    std::string pathUtf8{};
    std::string sha256{};
    std::uint64_t sizeBytes{};
    std::string modifiedTimeUtc{};
};

struct Tolerances final {
    double positionMm{0.01};
    double surfaceMm{0.01};
    double angularDegrees{0.01};
    double booleanFuzzyMm{0.001};
    double relativeProperty{1.0e-6};
};

struct GeometryStatistics final {
    Vector3 boundingBoxMinimumMm{};
    Vector3 boundingBoxMaximumMm{};
    Vector3 sizeMm{};
    double volumeMm3{};
    double surfaceAreaMm2{};
    Vector3 centerOfMassMm{};
    std::uint64_t solidCount{};
    std::uint64_t shellCount{};
    std::uint64_t faceCount{};
    std::uint64_t edgeCount{};
    std::uint64_t vertexCount{};
    Vector3 principalMoments{};
    std::array<Vector3, 3> principalAxes{};
};

struct PlacementResult final {
    // Absolute position convention: B - A.
    Vector3 translationBMinusAMm{};
    // Rigid rotation that maps B toward A for comparison.
    Quaternion rotationBToA{};
    Vector3 displayEulerDegrees{};
    double rotationAngleDegrees{};
    bool ambiguousBySymmetry{};
};

struct DeviationStatistics final {
    bool available{};
    double maximumMm{};
    double meanMm{};
    double rmsMm{};
    double percentile95Mm{};
    std::uint64_t sampleCount{};
    std::uint64_t triangleDistanceEvaluations{};
};

struct ExecutionMetadata final {
    std::string status{"NOT_STARTED"};
    std::string terminalPhase{};
    bool cancellationRequested{};
    bool allRequiredEvidenceComplete{};
};

struct CacheMetadata final {
    bool enabled{};
    bool hit{};
    std::string key{};
    std::uint64_t hits{};
    std::uint64_t misses{};
    std::uint64_t evictions{};
    std::uint64_t usedBytes{};
    std::uint64_t budgetBytes{};
};

struct Timing final {
    std::string phase{};
    double elapsedMilliseconds{};
};

struct Verdict final {
    std::string decision{};
    std::vector<std::string> reasons{};
};

struct ComponentRow final {
    std::string idA{};
    std::string idB{};
    std::string nameA{};
    std::string nameB{};
    std::string matchStatus{};
    std::string geometryStatus{};
    std::string positionStatus{};
    Vector3 translationBMinusAMm{};
    Quaternion rotationBToA{};
    double rotationAngleDegrees{};
    Vector3 boundingBoxSizeDifferenceMm{};
    double volumeDifferenceMm3{};
    double surfaceAreaDifferenceMm2{};
    DeviationStatistics deviation{};
    double confidence{};
};

// Additive DEV V1 extension. Whole-model verdict semantics remain unchanged;
// feature rows carry their own fail-closed evidence/result.
struct FeatureRow final {
    std::string idA{};
    std::string idB{};
    std::string ownerComponentIdA{};
    std::string ownerComponentIdB{};
    std::string type{};
    std::string evidenceStatus{};
    std::string result{};
    std::string reason{};
    Vector3 centerAAbsoluteMm{};
    Vector3 centerBAbsoluteMm{};
    Vector3 centerBAlignedMm{};
    // Both conventions are explicit B - A.
    Vector3 absoluteDifferenceBMinusAMm{};
    Vector3 alignedDifferenceBMinusAMm{};
    Vector3 axisA{};
    Vector3 axisB{};
    Vector3 axisBAligned{};
    double primarySizeAMm{};
    double primarySizeBMm{};
    double secondarySizeAMm{};
    double secondarySizeBMm{};
    double depthAMm{};
    double depthBMm{};
    double radiusAMm{};
    double radiusBMm{};
    double angleADegrees{};
    double angleBDegrees{};
    std::string profileA{};
    std::string profileB{};
    bool throughA{};
    bool throughB{};
    double positionToleranceMm{};
    double angularToleranceDegrees{};
    double confidence{};
    std::vector<std::uint32_t> faceIndicesA{};
    std::vector<std::uint32_t> faceIndicesB{};
};

struct Report final {
    // 1.0: whole-model and assembly canonical result.
    // 1.1: additive feature-level evidence; meanings of 1.0 fields are unchanged.
    std::string schemaVersion{"1.1"};
    std::string softwareVersion{};
    std::string algorithmVersion{};
    InputIdentity inputA{};
    InputIdentity inputB{};
    Tolerances tolerances{};
    ExecutionMetadata execution{};
    CacheMetadata cache{};
    GeometryStatistics statisticsA{};
    GeometryStatistics statisticsB{};
    PlacementResult placement{};
    DeviationStatistics deepDeviation{};
    std::vector<Timing> timings{};
    Verdict verdict{};
    std::vector<ComponentRow> components{};
    std::vector<FeatureRow> features{};
};

}  // namespace stepcompare::reporting
