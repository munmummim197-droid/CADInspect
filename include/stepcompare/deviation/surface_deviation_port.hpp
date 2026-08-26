#pragma once

#include <stepcompare/import/imported_model.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace stepcompare::deviation {

enum class SurfaceDeviationStatus {
    WithinTolerance,
    DeviationFound,
    Cancelled,
    NoSurfaceData,
    Error,
};

enum class SurfaceDeviationDiagnosticCode {
    MissingGeometryPayload,
    InvalidOptions,
    InvalidAlignmentTransform,
    TriangulationFailed,
    NoTriangles,
    Cancelled,
    OcctFailure,
    UnexpectedFailure,
};

struct SurfaceDeviationDiagnostic final {
    SurfaceDeviationDiagnosticCode code{
        SurfaceDeviationDiagnosticCode::UnexpectedFailure};
    std::string messageUtf8{};
};

struct SurfaceDeviationOptions final {
    double toleranceMm{0.01};
    double meshDeflectionMm{0.05};
    double meshAngularDeflectionDegrees{10.0};
    double percentile{95.0};
    std::size_t maximumSamplesPerDirection{100'000};
};

struct SurfaceDeviationRequest final {
    import::GeometryPayloadPtr geometryA{};
    import::GeometryPayloadPtr geometryB{};
    // Optional upstream alignment, always interpreted as B -> A.
    import::RigidTransformMm transformBToA{};
    SurfaceDeviationOptions options{};
    std::function<bool()> isCancelled{};
};

struct SurfaceDeviationResult final {
    SurfaceDeviationStatus status{SurfaceDeviationStatus::Error};
    double maximumMm{};
    double meanMm{};
    double rmsMm{};
    double percentileMm{};
    std::size_t samplesAToB{};
    std::size_t samplesBToA{};
    std::size_t trianglesA{};
    std::size_t trianglesB{};
    // Evidence that nearest queries were BVH-pruned rather than a mandatory
    // all-samples x all-triangles scan.
    std::size_t triangleDistanceEvaluations{};
    std::vector<SurfaceDeviationDiagnostic> diagnostics{};
};

class SurfaceDeviationPort {
public:
    virtual ~SurfaceDeviationPort() = default;
    [[nodiscard]] virtual SurfaceDeviationResult compare(
        const SurfaceDeviationRequest& request) noexcept = 0;
};

}  // namespace stepcompare::deviation
