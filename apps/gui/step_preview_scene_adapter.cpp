#include "step_preview_scene_adapter.hpp"

#include "step_preview_model.hpp"

#include "adapters/occt/occt_geometry_payload.hpp"

#include <stepcompare/viewer/occt_viewer_widget.hpp>

#include <TopLoc_Location.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace stepcompare::gui {
namespace {

gp_Trsf toOcctTransform(const std::array<double, 16>& m) {
    gp_Trsf result;
    result.SetValues(m[0], m[1], m[2], m[3],
                     m[4], m[5], m[6], m[7],
                     m[8], m[9], m[10], m[11]);
    return result;
}

}  // namespace

std::vector<stepcompare::viewer::ResultRowSnapshot>
StepPreviewSceneAdapter::display(
    const stepcompare::import::ImportedModel& model,
    const stepcompare::viewer::ModelSide side,
    stepcompare::viewer::OcctViewerWidget& viewer) const {
    using stepcompare::viewer::StableSelectionId;

    viewer.clearShapes(side);
    auto plan = buildPreviewScenePlan(model, side);

    std::unordered_map<std::string, const stepcompare::import::PartPrototype*>
        prototypes;
    prototypes.reserve(model.prototypes.size());
    for (const auto& prototype : model.prototypes) {
        prototypes.emplace(prototype.id, &prototype);
    }

    for (const auto& occurrence : plan.occurrences) {
        const auto prototype = prototypes.find(occurrence.prototypeId);
        if (prototype == prototypes.end()) {
            throw std::runtime_error("Preview node references an unknown prototype");
        }
        const TopoDS_Shape* sourceShape =
            stepcompare::adapters::occt::tryGetShape(prototype->second->geometry);
        if (sourceShape == nullptr || sourceShape->IsNull()) {
            throw std::runtime_error("Preview prototype has no OCCT shape payload");
        }

        // Moved() composes the occurrence transform on a lightweight presentation
        // copy. The payload shape and imported source transform remain immutable.
        const TopoDS_Shape located = sourceShape->Moved(
            TopLoc_Location(toOcctTransform(occurrence.worldTransform)));
        viewer.displayShape(located,
                            side,
                            StableSelectionId{occurrence.stableId.value()},
                            false,
                            false);
    }
    viewer.refreshPresentations();
    viewer.fitAll();
    return std::move(plan.rows);
}

}  // namespace stepcompare::gui
