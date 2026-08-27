#pragma once

#include "stepcompare/deep/deep_geometry_port.hpp"
#include "stepcompare/cache/cache_key.hpp"
#include "stepcompare/cache/memory_budget_cache.hpp"
#include "stepcompare/deviation/surface_deviation_port.hpp"
#include "stepcompare/domain/result.hpp"
#include "stepcompare/domain/types.hpp"
#include "stepcompare/import/step_import_port.hpp"
#include "stepcompare/reporting/report.hpp"
#include "stepcompare/feature/feature_recognition_port.hpp"

#include <cstddef>
#include <functional>
#include <optional>
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
    SurfaceDeviationFailed,
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
    std::optional<cache::FileIdentity> identityA{};
    std::optional<cache::FileIdentity> identityB{};
    std::string importConfiguration{"occt-xcaf-mm-v1"};
    bool enableCache{true};
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
                          deep::DeepGeometryPort& deepGeometry,
                          deviation::SurfaceDeviationPort* surfaceDeviation = nullptr,
                          std::size_t cacheBudgetBytes = 256U * 1024U * 1024U) noexcept;

    ComparisonCoordinator(import::StepImportPort& importer,
                          deep::DeepGeometryPort& deepGeometry,
                          deviation::SurfaceDeviationPort* surfaceDeviation,
                          feature::FeatureRecognitionPort* featureRecognition,
                          std::size_t cacheBudgetBytes = 256U * 1024U * 1024U) noexcept;

    [[nodiscard]] ComparisonResult compare(
        const ComparisonRequest& request) noexcept;

private:
    import::StepImportPort& importer_;
    deep::DeepGeometryPort& deepGeometry_;
    deviation::SurfaceDeviationPort* surfaceDeviation_{};
    feature::FeatureRecognitionPort* featureRecognition_{};
    cache::MemoryBudgetCache<ComparisonResult> cache_;
};

}  // namespace stepcompare::application
