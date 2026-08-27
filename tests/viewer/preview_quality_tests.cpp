#include "preview_quality.hpp"

#include "adapters/occt/occt_geometry_payload.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void policyBalancesDetailAndScale() {
    using namespace stepcompare::gui;
    const auto detail = choosePreviewQuality(1, 1);
    const auto balanced = choosePreviewQuality(800, 120);
    const auto scalable = choosePreviewQuality(5'000, 1);
    expect(detail.tier == PreviewQualityTier::Detail && detail.drawFeatureEdges,
           "single parts must use detail tessellation and feature edges");
    expect(balanced.tier == PreviewQualityTier::Balanced,
           "mid-sized assemblies must use balanced quality");
    expect(scalable.tier == PreviewQualityTier::Scalable &&
               !scalable.drawFeatureEdges,
           "5000 occurrences must use the scalable edge policy");
    expect(detail.angularDeflectionDegrees < scalable.angularDeflectionDegrees,
           "detail profiles must preserve curved surfaces more finely");
}

void deflectionIsStableAndBounded() {
    stepcompare::domain::GeometryStatistics statistics;
    statistics.boundingBox.maximum = {100.0, 80.0, 30.0};
    const auto detail = stepcompare::gui::choosePreviewQuality(1, 1);
    const double value =
        stepcompare::gui::previewLinearDeflectionMm(statistics, detail);
    expect(std::isfinite(value) && value >= 0.01 && value <= 0.20,
           "detail deflection must be finite and bounded");
}

void meshesOnePrototypeAndReusesOccurrences() {
    const auto plate = BRepPrimAPI_MakeBox(100.0, 70.0, 20.0).Shape();
    const auto hole = BRepPrimAPI_MakeCylinder(
        gp_Ax2(gp_Pnt(35.0, 35.0, -1.0), gp_Dir(0.0, 0.0, 1.0)),
        8.0,
        22.0)
                          .Shape();
    const auto drilled = BRepAlgoAPI_Cut(plate, hole).Shape();

    stepcompare::import::ImportedModel model;
    stepcompare::import::PartPrototype prototype;
    prototype.id = "drilled";
    prototype.statistics.boundingBox.maximum = {100.0, 70.0, 20.0};
    prototype.geometry =
        std::make_shared<stepcompare::adapters::occt::OcctGeometryPayload>(drilled);
    model.prototypes.push_back(std::move(prototype));
    for (int index = 0; index < 5'000; ++index) {
        stepcompare::import::AssemblyNode node;
        node.id = std::to_string(index);
        node.prototypeId = "drilled";
        model.nodes.push_back(std::move(node));
    }

    const auto summary = stepcompare::gui::preparePreviewMeshes(model);
    expect(summary.policy.tier == stepcompare::gui::PreviewQualityTier::Scalable,
           "5000 occurrences must retain scalable quality");
    expect(summary.meshedPrototypeCount == 1 &&
               summary.failedPrototypeCount == 0,
           "the unique drilled prototype must be tessellated once");
    expect(summary.reusedOccurrenceCount == 4'999,
           "repeated instances must reuse the prototype mesh");
    expect(summary.triangleCount > 0,
           "drilled geometry must produce a real triangulation");

    std::size_t triangulatedFaces{};
    const auto* shape = stepcompare::adapters::occt::tryGetShape(
        model.prototypes.front().geometry);
    for (TopExp_Explorer explorer(*shape, TopAbs_FACE); explorer.More();
         explorer.Next()) {
        TopLoc_Location location;
        if (!BRep_Tool::Triangulation(
                 TopoDS::Face(explorer.Current()), location)
                 .IsNull()) {
            ++triangulatedFaces;
        }
    }
    expect(triangulatedFaces >= 7,
           "the through-hole faces must survive preview tessellation");
}

}  // namespace

int main() {
    policyBalancesDetailAndScale();
    deflectionIsStableAndBounded();
    meshesOnePrototypeAndReusesOccurrences();
    if (failures != 0) {
        std::cerr << failures << " preview quality assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All preview quality tests passed\n";
    return EXIT_SUCCESS;
}
