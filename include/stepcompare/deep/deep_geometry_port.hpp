#pragma once

#include "stepcompare/import/imported_model.hpp"

#include <string>
#include <vector>

namespace stepcompare::deep {

enum class DeepGeometryStatus {
    SameGeometry,
    GeometryChanged,
    AlignmentNotProven,
    OpenShellUnsupported,
    Error,
};

enum class DeepDiagnosticCode {
    MissingGeometryPayload,
    InvalidTolerance,
    InvalidClosedSolid,
    AlignmentNotProven,
    BooleanOperationFailed,
    OcctFailure,
    UnexpectedFailure,
};

struct DeepDiagnostic final {
    DeepDiagnosticCode code{DeepDiagnosticCode::UnexpectedFailure};
    std::string messageUtf8;
};

struct DeepGeometryOptions final {
    double booleanFuzzyMm{0.001};
    double relativeVolumeTolerance{1.0e-7};
};

struct DeepGeometryRequest final {
    import::GeometryPayloadPtr geometryA;
    import::GeometryPayloadPtr geometryB;
    DeepGeometryOptions options{};
};

struct DeepGeometryResult final {
    DeepGeometryStatus status{DeepGeometryStatus::Error};
    import::RigidTransformMm transformBToA{};
    double volumeAMm3{};
    double volumeBMm3{};
    double commonVolumeMm3{};
    double symmetricDifferenceVolumeMm3{};
    bool alignmentProven{false};
    std::vector<DeepDiagnostic> diagnostics;
};

class DeepGeometryPort {
public:
    virtual ~DeepGeometryPort() = default;
    [[nodiscard]] virtual DeepGeometryResult compareAligned(
        const DeepGeometryRequest& request) noexcept = 0;
};

}  // namespace stepcompare::deep
