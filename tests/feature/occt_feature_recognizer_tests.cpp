#include <stepcompare/feature/occt_feature_recognizer.hpp>

#include "adapters/occt/occt_geometry_payload.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

stepcompare::feature::FeatureRecognitionResult recognize(
    const TopoDS_Shape& shape) {
    const auto payload =
        std::make_shared<stepcompare::adapters::occt::OcctGeometryPayload>(shape);
    stepcompare::feature::OcctFeatureRecognizer recognizer;
    return recognizer.recognize(payload, 0.01, 0.05);
}

bool contains(const stepcompare::feature::FeatureRecognitionResult& result,
              const stepcompare::feature::FeatureType type,
              const stepcompare::feature::RecognitionEvidence evidence =
                  stepcompare::feature::RecognitionEvidence::GeometryProven) {
    for (const auto& feature : result.features) {
        if (feature.type == type && feature.evidence == evidence) {
            return true;
        }
    }
    return false;
}

const stepcompare::feature::RecognizedFeature* findFeature(
    const stepcompare::feature::FeatureRecognitionResult& result,
    const stepcompare::feature::FeatureType type,
    const stepcompare::feature::RecognitionEvidence evidence =
        stepcompare::feature::RecognitionEvidence::GeometryProven) {
    for (const auto& feature : result.features) {
        if (feature.type == type && feature.evidence == evidence) {
            return &feature;
        }
    }
    return nullptr;
}

TopoDS_Shape plate() {
    return BRepPrimAPI_MakeBox(100.0, 70.0, 20.0).Shape();
}

TopoDS_Shape cylinder(const double x,
                      const double y,
                      const double z,
                      const double radius,
                      const double depth) {
    return BRepPrimAPI_MakeCylinder(
               gp_Ax2(gp_Pnt(x, y, z), gp_Dir(0.0, 0.0, 1.0)),
               radius,
               depth)
        .Shape();
}

void recognizesThroughAndBlindWithoutHistory() {
    const auto through = BRepAlgoAPI_Cut(plate(), cylinder(30, 35, -1, 6, 22)).Shape();
    const auto blind = BRepAlgoAPI_Cut(plate(), cylinder(30, 35, 10, 6, 11)).Shape();
    const auto throughResult = recognize(through);
    const auto blindResult = recognize(blind);
    expect(contains(throughResult, stepcompare::feature::FeatureType::ThroughHole),
           "B-Rep through hole must be geometrically recognized");
    expect(contains(blindResult, stepcompare::feature::FeatureType::BlindPocket),
           "a planar end cap must distinguish a blind cylindrical pocket");
    expect(!contains(blindResult, stepcompare::feature::FeatureType::ThroughHole),
           "blind geometry must not receive a false through-hole result");

    const auto rectangular = BRepAlgoAPI_Cut(
        plate(),
        BRepPrimAPI_MakeBox(gp_Pnt(20, 20, 12), 32, 18, 9).Shape()).Shape();
    const auto rectangularResult = recognize(rectangular);
    const auto* pocket = findFeature(
        rectangularResult, stepcompare::feature::FeatureType::BlindPocket);
    expect(pocket != nullptr,
           "five connected internal planar faces must prove a rectangular blind pocket");
    if (pocket != nullptr) {
        expect(pocket->primarySizeMm > 17.9 &&
                   pocket->secondarySizeMm > 31.9 &&
                   pocket->depthMm > 7.9 && !pocket->through,
               "rectangular blind pocket must expose width, length and depth evidence");
    }
}

void recognizesCounterboreAndSlot() {
    TopoDS_Shape steppedTool = cylinder(28, 35, -1, 5, 22);
    steppedTool = BRepAlgoAPI_Fuse(steppedTool, cylinder(28, 35, 15, 9, 6)).Shape();
    const auto counterbore = BRepAlgoAPI_Cut(plate(), steppedTool).Shape();
    expect(contains(recognize(counterbore),
                    stepcompare::feature::FeatureType::Counterbore),
           "coaxial stepped cylindrical B-Rep must be recognized as counterbore");

    TopoDS_Shape slotTool = cylinder(30, 35, -1, 6, 22);
    slotTool = BRepAlgoAPI_Fuse(slotTool, cylinder(65, 35, -1, 6, 22)).Shape();
    slotTool = BRepAlgoAPI_Fuse(
                   slotTool,
                   BRepPrimAPI_MakeBox(gp_Pnt(30, 29, -1), 35, 12, 22).Shape())
                   .Shape();
    const auto slot = BRepAlgoAPI_Cut(plate(), slotTool).Shape();
    const auto slotResult = recognize(slot);
    expect(contains(slotResult, stepcompare::feature::FeatureType::Slot),
           "two semicylindrical ends must be recognized as an obround slot");
    expect(!contains(slotResult, stepcompare::feature::FeatureType::ThroughHole),
           "trimmed slot walls must not be promoted to false through holes");
}

void recognizesFilletChamferKeywayAndAmbiguousRecess() {
    const TopoDS_Shape base = BRepPrimAPI_MakeBox(70.0, 50.0, 20.0).Shape();
    BRepFilletAPI_MakeFillet fillet(base);
    TopExp_Explorer edge(base, TopAbs_EDGE);
    fillet.Add(4.0, TopoDS::Edge(edge.Current()));
    const auto filletResult = recognize(fillet.Shape());
    expect(contains(filletResult, stepcompare::feature::FeatureType::Fillet),
           "a real OCCT blend surface must be recognized as a fillet");
    for (const auto& feature : filletResult.features) {
        if (feature.type == stepcompare::feature::FeatureType::Fillet) {
            expect(!feature.through &&
                       feature.profile.find("BLEND") != std::string::npos,
                   "through/blind must remain non-applicable to fillets");
        }
    }

    TopoDS_Shape chamferTool = cylinder(35, 25, -1, 5, 22);
    chamferTool = BRepAlgoAPI_Fuse(
                      chamferTool,
                      BRepPrimAPI_MakeCone(
                          gp_Ax2(gp_Pnt(35, 25, 16), gp_Dir(0, 0, 1)),
                          5.0, 9.0, 4.0)
                          .Shape())
                      .Shape();
    const auto chamfered = BRepAlgoAPI_Cut(base, chamferTool).Shape();
    expect(contains(recognize(chamfered),
                    stepcompare::feature::FeatureType::Chamfer),
           "a conical transition in a machined hole must be recognized as chamfer");

    const auto shaft = cylinder(0, 0, 0, 20, 80);
    const auto keywayTool =
        BRepPrimAPI_MakeBox(gp_Pnt(-5, 10, 20), 10, 15, 40).Shape();
    const auto keyway = BRepAlgoAPI_Cut(shaft, keywayTool).Shape();
    const auto keywayResult = recognize(keyway);
    const auto* keywayFeature = findFeature(
        keywayResult, stepcompare::feature::FeatureType::Keyway);
    expect(keywayFeature != nullptr,
           "connected planar shaft recess topology must prove a keyway");
    if (keywayFeature != nullptr) {
        expect(keywayFeature->primarySizeMm > 9.9 &&
                   keywayFeature->secondarySizeMm > 39.9 &&
                   keywayFeature->depthMm > 9.9 &&
                   keywayFeature->profile == "RECTANGULAR_KEYWAY",
               "keyway must expose width, length, depth and profile evidence");
    }

    const auto ambiguousTool =
        BRepPrimAPI_MakeBox(gp_Pnt(85, 20, 12), 20, 20, 9).Shape();
    const auto ambiguous = BRepAlgoAPI_Cut(plate(), ambiguousTool).Shape();
    expect(contains(recognize(ambiguous),
                    stepcompare::feature::FeatureType::BlindPocket,
                    stepcompare::feature::RecognitionEvidence::Ambiguous),
           "open planar notch without a closed five-face recess must remain ambiguous");
}

}  // namespace

int main() {
    recognizesThroughAndBlindWithoutHistory();
    recognizesCounterboreAndSlot();
    recognizesFilletChamferKeywayAndAmbiguousRecess();
    if (failures != 0) {
        std::cerr << failures << " feature recognition assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All OCCT feature recognition tests passed\n";
    return EXIT_SUCCESS;
}
