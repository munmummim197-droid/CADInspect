#include "preview_quality.hpp"

#include "adapters/occt/occt_geometry_payload.hpp"

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>

namespace stepcompare::gui {
namespace {

std::uint64_t triangleCount(const TopoDS_Shape& shape) {
    std::uint64_t result{};
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More();
         explorer.Next()) {
        TopLoc_Location location;
        const auto triangulation = BRep_Tool::Triangulation(
            TopoDS::Face(explorer.Current()), location);
        if (!triangulation.IsNull()) {
            result += static_cast<std::uint64_t>(triangulation->NbTriangles());
        }
    }
    return result;
}

}  // namespace

PreviewQualityPolicy choosePreviewQuality(
    const std::size_t occurrenceCount,
    const std::size_t prototypeCount) noexcept {
    if (occurrenceCount > 2'000U || prototypeCount > 500U) {
        return {.tier = PreviewQualityTier::Scalable,
                .relativeLinearDeflection = 0.003,
                .minimumLinearDeflectionMm = 0.05,
                .maximumLinearDeflectionMm = 0.80,
                .angularDeflectionDegrees = 18.0,
                .drawFeatureEdges = false};
    }
    if (occurrenceCount > 250U || prototypeCount > 100U) {
        return {.tier = PreviewQualityTier::Balanced,
                .relativeLinearDeflection = 0.0015,
                .minimumLinearDeflectionMm = 0.02,
                .maximumLinearDeflectionMm = 0.40,
                .angularDeflectionDegrees = 12.0,
                .drawFeatureEdges = true};
    }
    return {};
}

double previewLinearDeflectionMm(
    const stepcompare::domain::GeometryStatistics& statistics,
    const PreviewQualityPolicy& policy) noexcept {
    const auto size = statistics.boundingBox.size();
    const double diagonal = std::hypot(size.x, size.y, size.z);
    const double scaled = std::isfinite(diagonal) && diagonal > 0.0
                              ? diagonal * policy.relativeLinearDeflection
                              : policy.minimumLinearDeflectionMm;
    return std::clamp(scaled,
                      policy.minimumLinearDeflectionMm,
                      policy.maximumLinearDeflectionMm);
}

PreviewMeshSummary preparePreviewMeshes(
    stepcompare::import::ImportedModel& model) noexcept {
    PreviewMeshSummary summary;
    for (const auto& node : model.nodes) {
        summary.occurrenceCount += node.prototypeId.has_value() ? 1U : 0U;
    }
    summary.prototypeCount = model.prototypes.size();
    summary.reusedOccurrenceCount =
        summary.occurrenceCount > summary.prototypeCount
            ? summary.occurrenceCount - summary.prototypeCount
            : 0U;
    summary.policy = choosePreviewQuality(summary.occurrenceCount,
                                          summary.prototypeCount);

    const auto started = std::chrono::steady_clock::now();
    for (const auto& prototype : model.prototypes) {
        const TopoDS_Shape* shape =
            stepcompare::adapters::occt::tryGetShape(prototype.geometry);
        if (shape == nullptr || shape->IsNull()) {
            ++summary.failedPrototypeCount;
            continue;
        }
        try {
            const double linearDeflection =
                previewLinearDeflectionMm(prototype.statistics, summary.policy);
            const double angularDeflection =
                summary.policy.angularDeflectionDegrees * std::numbers::pi / 180.0;
            BRepMesh_IncrementalMesh mesh(
                *shape, linearDeflection, false, angularDeflection, false);
            if (!mesh.IsDone()) {
                ++summary.failedPrototypeCount;
                continue;
            }
            ++summary.meshedPrototypeCount;
            summary.triangleCount += triangleCount(*shape);
        } catch (const Standard_Failure&) {
            ++summary.failedPrototypeCount;
        } catch (...) {
            ++summary.failedPrototypeCount;
        }
    }
    summary.elapsedMilliseconds =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started)
            .count();
    return summary;
}

const char* previewQualityTierName(const PreviewQualityTier tier) noexcept {
    switch (tier) {
        case PreviewQualityTier::Detail:
            return "DETAIL";
        case PreviewQualityTier::Balanced:
            return "BALANCED";
        case PreviewQualityTier::Scalable:
            return "SCALABLE";
    }
    return "SCALABLE";
}

}  // namespace stepcompare::gui
