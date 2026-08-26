#pragma once

#include <vector>

#include <stepcompare/import/imported_model.hpp>
#include <stepcompare/viewer/selection_presenter.hpp>
#include <stepcompare/viewer/viewer_state.hpp>

namespace stepcompare::viewer {
class OcctViewerWidget;
}

namespace stepcompare::gui {

class StepPreviewSceneAdapter final {
public:
    [[nodiscard]] std::vector<stepcompare::viewer::ResultRowSnapshot> display(
        const stepcompare::import::ImportedModel& model,
        stepcompare::viewer::ModelSide side,
        stepcompare::viewer::OcctViewerWidget& viewer) const;
};

}  // namespace stepcompare::gui
