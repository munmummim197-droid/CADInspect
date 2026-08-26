#pragma once

#include <array>
#include <string>
#include <vector>

#include <stepcompare/import/imported_model.hpp>
#include <stepcompare/viewer/selection_presenter.hpp>
#include <stepcompare/viewer/viewer_state.hpp>

namespace stepcompare::gui {

struct PreviewOccurrence final {
    stepcompare::viewer::StableSelectionId stableId;
    std::string prototypeId;
    std::array<double, 16> worldTransform{};
};

struct PreviewScenePlan final {
    std::vector<stepcompare::viewer::ResultRowSnapshot> rows;
    std::vector<PreviewOccurrence> occurrences;
};

[[nodiscard]] PreviewScenePlan buildPreviewScenePlan(
    const stepcompare::import::ImportedModel& model,
    stepcompare::viewer::ModelSide side);

}  // namespace stepcompare::gui
