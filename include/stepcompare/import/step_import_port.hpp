#pragma once

#include "stepcompare/import/imported_model.hpp"

#include <string>
#include <utility>
#include <vector>

namespace stepcompare::import {

enum class ImportDiagnosticSeverity {
    Warning,
    Error,
};

enum class ImportDiagnosticCode {
    InvalidUtf8Path,
    FileOpenFailed,
    StepReadFailed,
    StepTransferFailed,
    UnitMetadataMissing,
    UnitNormalizationUnverified,
    EmptyDocument,
    GeometryAnalysisFailed,
    OcctFailure,
    UnexpectedFailure,
};

struct ImportDiagnostic final {
    ImportDiagnosticSeverity severity{ImportDiagnosticSeverity::Error};
    ImportDiagnosticCode code{ImportDiagnosticCode::UnexpectedFailure};
    std::string messageUtf8;
};

struct StepImportRequest final {
    std::u8string sourcePathUtf8;
};

struct StepImportResult final {
    ImportedModel model{};
    std::vector<ImportDiagnostic> diagnostics;

    [[nodiscard]] bool succeeded() const noexcept {
        if (model.rootNodeIds.empty()) {
            return false;
        }
        for (const auto& diagnostic : diagnostics) {
            if (diagnostic.severity == ImportDiagnosticSeverity::Error) {
                return false;
            }
        }
        return model.lengthUnit.status ==
               UnitNormalizationStatus::NormalizedToMillimetres;
    }
};

class StepImportPort {
public:
    virtual ~StepImportPort() = default;
    [[nodiscard]] virtual StepImportResult importStep(
        const StepImportRequest& request) noexcept = 0;
};

}  // namespace stepcompare::import
