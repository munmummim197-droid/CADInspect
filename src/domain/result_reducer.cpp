#include <stepcompare/domain/result.hpp>

#include <utility>

namespace stepcompare::domain {

Verdict reduceVerdict(const EvidenceSummary& evidence) {
    if (evidence.stepImportFailed) {
        return {Decision::Error, {ReasonCode::StepImportFailed}};
    }
    if (evidence.deepCheckFailed) {
        return {Decision::Error, {ReasonCode::DeepCheckFailed}};
    }

    std::vector<ReasonCode> structuralDifferences;
    if (evidence.componentMissing) {
        structuralDifferences.push_back(ReasonCode::ComponentMissing);
    }
    if (evidence.componentAdded) {
        structuralDifferences.push_back(ReasonCode::ComponentAdded);
    }
    if (!structuralDifferences.empty()) {
        return {Decision::Fail, std::move(structuralDifferences)};
    }

    if (evidence.geometry == GeometryStatus::ChangedProven) {
        return {Decision::Fail, {ReasonCode::GeometryChanged}};
    }

    if (evidence.alignmentAmbiguous) {
        return {Decision::Check, {ReasonCode::AlignmentAmbiguous}};
    }
    if (evidence.rotationAmbiguousBySymmetry) {
        return {Decision::Check, {ReasonCode::RotationAmbiguousBySymmetry}};
    }
    if (!evidence.allRequiredStagesComplete ||
        evidence.geometry != GeometryStatus::SameProven ||
        evidence.position == PositionStatus::Unknown) {
        return {Decision::Check, {ReasonCode::EvidenceIncomplete}};
    }

    if (evidence.position == PositionStatus::Same) {
        return {Decision::Pass, {ReasonCode::SameGeometrySamePosition}};
    }

    return {Decision::Fail, {ReasonCode::SameGeometryPositionChanged}};
}

}  // namespace stepcompare::domain
