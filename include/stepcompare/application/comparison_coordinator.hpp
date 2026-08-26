#pragma once

#include "stepcompare/deep/deep_geometry_port.hpp"
#include "stepcompare/domain/result.hpp"
#include "stepcompare/domain/types.hpp"
#include "stepcompare/import/step_import_port.hpp"
#include "stepcompare/reporting/report.hpp"

#include <cstddef>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>

namespace stepcompare::application {

enum class ComparisonRunStatus {
    Completed,
    InputError,
    ProcessingError,
    Cancelled,
};

enum class ComparisonDiagnosticCode {
    ImportAFailed,
    ImportBFailed,
    AssemblyIndexAFailed,
    AssemblyIndexBFailed,
    DeepComparisonFailed,
    Cancelled,
    InternalFailure,
};

enum class ComparisonPhase {
    ImportA,
    ImportB,
    AssemblyIndex,
    Matching,
    Complete,
};

struct ComparisonProgress final {
    ComparisonPhase phase{ComparisonPhase::ImportA};
    std::size_t completedStages{};
    std::size_t totalStages{5};
};

using ComparisonProgressCallback =
    std::function<void(const ComparisonProgress&)>;

struct ComparisonDiagnostic final {
    ComparisonDiagnosticCode code{ComparisonDiagnosticCode::InternalFailure};
    std::string messageUtf8;
};

struct ComparisonRequest final {
    std::u8string inputAUtf8;
    std::u8string inputBUtf8;
    domain::ToleranceSet tolerances{};
    bool deep{false};
    std::stop_token cancellation{};
    ComparisonProgressCallback progress{};
};

struct ComparisonResult final {
    ComparisonRunStatus status{ComparisonRunStatus::ProcessingError};
    domain::Verdict verdict{};
    reporting::Report report{};
    std::vector<ComparisonDiagnostic> diagnostics;
};

class ComparisonCoordinator final {
public:
    ComparisonCoordinator(import::StepImportPort& importer,
                          deep::DeepGeometryPort& deepGeometry) noexcept;

    [[nodiscard]] ComparisonResult compare(
        const ComparisonRequest& request) noexcept;

private:
    import::StepImportPort& importer_;
    deep::DeepGeometryPort& deepGeometry_;
};

}  // namespace stepcompare::application
