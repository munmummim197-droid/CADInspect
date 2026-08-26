#pragma once

#include <vector>

namespace stepcompare::domain {

enum class Decision {
    Pass,
    Fail,
    Check,
    Error,
};

enum class GeometryStatus {
    SameProven,
    ChangedProven,
    ProbableSame,
    Unknown,
};

enum class PositionStatus {
    Same,
    Translated,
    Rotated,
    TranslatedAndRotated,
    Unknown,
};

enum class ReasonCode {
    SameGeometrySamePosition,
    SameGeometryPositionChanged,
    GeometryChanged,
    ComponentMissing,
    ComponentAdded,
    AlignmentAmbiguous,
    RotationAmbiguousBySymmetry,
    EvidenceIncomplete,
    StepImportFailed,
    DeepCheckFailed,
};

struct EvidenceSummary final {
    GeometryStatus geometry{GeometryStatus::Unknown};
    PositionStatus position{PositionStatus::Unknown};
    bool allRequiredStagesComplete{false};
    bool componentMissing{false};
    bool componentAdded{false};
    bool alignmentAmbiguous{false};
    bool rotationAmbiguousBySymmetry{false};
    bool stepImportFailed{false};
    bool deepCheckFailed{false};
};

struct Verdict final {
    Decision decision{Decision::Check};
    std::vector<ReasonCode> reasons{};
};

[[nodiscard]] Verdict reduceVerdict(const EvidenceSummary& evidence);

}  // namespace stepcompare::domain

