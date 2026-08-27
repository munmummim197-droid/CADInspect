#include "stepcompare/application/comparison_coordinator.hpp"
#include "stepcompare/deep/occt_deep_geometry_engine.hpp"
#include "stepcompare/deviation/occt_surface_deviation_engine.hpp"
#include "stepcompare/feature/occt_feature_recognizer.hpp"
#include "stepcompare/import/occt_step_importer.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Builder.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace {

enum class Kind { ThroughHole, Counterbore, Slot, Keyway, BlindPocket, Fillet, Chamfer };

struct KindSpec final {
    Kind kind;
    std::string_view name;
    std::string_view reportType;
};

constexpr std::array kKinds{
    KindSpec{Kind::ThroughHole, "through-hole", "THROUGH_HOLE"},
    KindSpec{Kind::Counterbore, "counterbore", "COUNTERBORE"},
    KindSpec{Kind::Slot, "slot", "SLOT"},
    KindSpec{Kind::Keyway, "keyway", "KEYWAY"},
    KindSpec{Kind::BlindPocket, "blind-pocket", "BLIND_POCKET"},
    KindSpec{Kind::Fillet, "fillet", "FILLET"},
    KindSpec{Kind::Chamfer, "chamfer", "CHAMFER"},
};

int failures{};

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

TopoDS_Shape plate() {
    return BRepPrimAPI_MakeBox(100.0, 70.0, 20.0).Shape();
}

TopoDS_Shape cylinder(double x, double y, double z, double radius, double depth) {
    return BRepPrimAPI_MakeCylinder(
               gp_Ax2(gp_Pnt(x, y, z), gp_Dir(0.0, 0.0, 1.0)), radius, depth)
        .Shape();
}

TopoDS_Shape moved(const TopoDS_Shape& shape, double x) {
    gp_Trsf transform;
    transform.SetTranslation(gp_Vec(x, 0.0, 0.0));
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
}

TopoDS_Shape featureShape(Kind kind, double featureMove, double dimensionDelta) {
    const double x = 31.0 + featureMove;
    switch (kind) {
    case Kind::ThroughHole:
        return BRepAlgoAPI_Cut(
                   plate(), cylinder(x, 29.0, -1.0, 5.0 + dimensionDelta, 22.0))
            .Shape();
    case Kind::Counterbore: {
        auto tool = cylinder(x, 29.0, -1.0, 4.0, 22.0);
        tool = BRepAlgoAPI_Fuse(
                   tool, cylinder(x, 29.0, 15.0, 8.0 + dimensionDelta, 6.0))
                   .Shape();
        return BRepAlgoAPI_Cut(plate(), tool).Shape();
    }
    case Kind::Slot: {
        const double radius = 5.0 + dimensionDelta;
        auto tool = cylinder(x, 29.0, -1.0, radius, 22.0);
        tool = BRepAlgoAPI_Fuse(tool, cylinder(x + 27.0, 29.0, -1.0, radius, 22.0)).Shape();
        tool = BRepAlgoAPI_Fuse(
                   tool,
                   BRepPrimAPI_MakeBox(gp_Pnt(x, 29.0 - radius, -1.0),
                                       27.0, radius * 2.0, 22.0)
                       .Shape())
                   .Shape();
        return BRepAlgoAPI_Cut(plate(), tool).Shape();
    }
    case Kind::Keyway: {
        const auto shaft = cylinder(0.0, 0.0, 0.0, 20.0, 80.0);
        const auto tool = BRepPrimAPI_MakeBox(
                              gp_Pnt(-5.0 - dimensionDelta * 0.5,
                                     10.0, 20.0 + featureMove),
                              10.0 + dimensionDelta, 15.0, 40.0)
                              .Shape();
        return BRepAlgoAPI_Cut(shaft, tool).Shape();
    }
    case Kind::BlindPocket:
        return BRepAlgoAPI_Cut(
                   plate(),
                   BRepPrimAPI_MakeBox(gp_Pnt(x, 21.0, 12.0 - dimensionDelta),
                                       30.0, 18.0, 9.0 + dimensionDelta)
                       .Shape())
            .Shape();
    case Kind::Fillet: {
        const auto raw = BRepPrimAPI_MakeBox(73.0, 51.0, 20.0).Shape();
        const auto base = BRepAlgoAPI_Cut(
                              raw, cylinder(13.0, 17.0, -1.0, 2.0, 22.0))
                              .Shape();
        BRepFilletAPI_MakeFillet fillet(base);
        TopExp_Explorer edge(base, TopAbs_EDGE);
        // Opposite parallel edge with the same length: this is a pure local
        // feature move, not a simultaneous fillet-depth change.
        const int desired = featureMove == 0.0 ? 0 : 2;
        for (int index = 0; index < desired && edge.More(); ++index) {
            edge.Next();
        }
        if (edge.More()) {
            fillet.Add(3.0 + dimensionDelta, TopoDS::Edge(edge.Current()));
        }
        return fillet.Shape();
    }
    case Kind::Chamfer: {
        auto tool = cylinder(x, 27.0, -1.0, 4.0, 22.0);
        tool = BRepAlgoAPI_Fuse(
                   tool,
                   BRepPrimAPI_MakeCone(
                       gp_Ax2(gp_Pnt(x, 27.0, 16.0), gp_Dir(0.0, 0.0, 1.0)),
                       4.0, 8.0 + dimensionDelta, 4.0)
                       .Shape())
                   .Shape();
        return BRepAlgoAPI_Cut(plate(), tool).Shape();
    }
    }
    return {};
}

TopoDS_Shape negativeShape(Kind kind) {
    return kind == Kind::Keyway ? cylinder(0.0, 0.0, 0.0, 20.0, 80.0)
                                : plate();
}

bool writeStep(const std::filesystem::path& path, const TopoDS_Shape& shape) {
    STEPControl_Writer writer;
    if (writer.Transfer(shape, STEPControl_AsIs) != IFSelect_RetDone) {
        return false;
    }
    std::ofstream stream(path, std::ios::binary);
    return stream && writer.WriteStream(stream) == IFSelect_RetDone;
}

double length(const stepcompare::reporting::Vector3& value) {
    return std::hypot(value.x, value.y, value.z);
}

const stepcompare::reporting::FeatureRow* findTarget(
    const stepcompare::application::ComparisonResult& result,
    std::string_view type) {
    for (const auto& row : result.report.features) {
        if (row.type == type) {
            return &row;
        }
    }
    return nullptr;
}

stepcompare::application::ComparisonResult compare(
    const std::filesystem::path& pathA,
    const std::filesystem::path& pathB) {
    stepcompare::import::OcctStepImporter importer;
    stepcompare::deep::OcctDeepGeometryEngine deep;
    stepcompare::deviation::OcctSurfaceDeviationEngine surface;
    stepcompare::feature::OcctFeatureRecognizer recognizer;
    stepcompare::application::ComparisonCoordinator coordinator(
        importer, deep, &surface, &recognizer);
    stepcompare::application::ComparisonRequest request;
    request.inputAUtf8 = pathA.u8string();
    request.inputBUtf8 = pathB.u8string();
    request.deep = true;
    request.enableCache = false;
    request.tolerances.positionMm = 0.5;
    request.tolerances.surfaceMm = 0.05;
    request.tolerances.angularDegrees = 0.1;
    return coordinator.compare(request);
}

void runCase(const std::filesystem::path& root,
             const KindSpec& spec,
             std::string_view caseName,
             const TopoDS_Shape& a,
             const TopoDS_Shape& b) {
    const auto directory = root / spec.name / caseName;
    std::filesystem::create_directories(directory);
    const auto pathA = directory / "A.step";
    const auto pathB = directory / "B.step";
    expect(writeStep(pathA, a) && writeStep(pathB, b),
           std::string(spec.name) + "/" + std::string(caseName) + " STEP write");
    const auto result = compare(pathA, pathB);
    expect(result.status == stepcompare::application::ComparisonRunStatus::Completed,
           std::string(spec.name) + "/" + std::string(caseName) + " completed");
    const auto* row = findTarget(result, spec.reportType);
    expect(row != nullptr,
           std::string(spec.name) + "/" + std::string(caseName) + " target row");
    if (row == nullptr) {
        return;
    }
    const std::string label = std::string(spec.name) + "/" + std::string(caseName);
    if (caseName == "SAME_FEATURE_SAME_POSITION") {
        expect(row->result == "PASS", label + " must PASS");
    } else if (caseName == "DIMENSION_CHANGED") {
        expect(row->result == "FAIL" && row->reason.find("_CHANGED") != std::string::npos,
               label + " must carry explicit changed reason");
    } else if (caseName == "WHOLE_PART_MOVED") {
        expect(length(row->absoluteDifferenceBMinusAMm) > 4.0,
               label + " absolute delta must retain owner move");
        expect(length(row->alignedDifferenceBMinusAMm) <= 0.5 && row->result == "PASS",
               label + " aligned delta must vanish and PASS");
    } else if (caseName == "NEGATIVE_OR_AMBIGUOUS") {
        expect((row->result == "FAIL" && row->reason == "FEATURE_MISSING") ||
                   (row->result == "CHECK" && row->reason == "FEATURE_AMBIGUOUS"),
               label + " must fail closed as missing or ambiguous");
    } else {
        expect(row->result == "FAIL" && row->reason == "FEATURE_POSITION_CHANGED" &&
                   length(row->alignedDifferenceBMinusAMm) > 0.5,
               label + " must preserve local feature movement after alignment");
    }
    std::cout << "PHYSICAL_FEATURE_CASE=" << spec.name << '/' << caseName
              << " RESULT=" << row->result << " REASON=" << row->reason
              << " ABS_DELTA_MM=" << length(row->absoluteDifferenceBMinusAMm)
              << " ALIGNED_DELTA_MM=" << length(row->alignedDifferenceBMinusAMm)
              << '\n';
}

void physicalFeatureCancellation(const std::filesystem::path& root) {
    const auto directory = root / "cancellation";
    std::filesystem::create_directories(directory);
    const auto path = directory / "feature-heavy.step";
    BRep_Builder builder;
    TopoDS_Compound tools;
    builder.MakeCompound(tools);
    for (int row = 0; row < 8; ++row) {
        for (int column = 0; column < 12; ++column) {
            builder.Add(tools, cylinder(8.0 + column * 17.0,
                                        8.0 + row * 17.0,
                                        -1.0, 3.0, 22.0));
        }
    }
    const auto heavy = BRepAlgoAPI_Cut(
                           BRepPrimAPI_MakeBox(210.0, 145.0, 20.0).Shape(),
                           tools)
                           .Shape();
    expect(writeStep(path, heavy), "feature-heavy cancellation STEP write");
    stepcompare::import::OcctStepImporter importer;
    const auto imported = importer.importStep({path.u8string()});
    expect(imported.succeeded() && !imported.model.prototypes.empty(),
           "feature-heavy cancellation STEP import");
    if (!imported.succeeded() || imported.model.prototypes.empty()) {
        return;
    }
    std::stop_source cancellation;
    std::atomic_bool started{};
    std::jthread requester([&] {
        while (!started.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        cancellation.request_stop();
    });
    stepcompare::feature::OcctFeatureRecognizer recognizer;
    started.store(true, std::memory_order_release);
    const auto recognized = recognizer.recognize(
        imported.model.prototypes.front().geometry, 0.05, 0.1,
        cancellation.get_token());
    expect(recognized.cancelled && !recognized.completed,
           "physical feature-heavy recognition must honor cancellation without stale completion");
    std::cout << "FEATURE_CANCELLATION_PHYSICAL="
              << (recognized.cancelled && !recognized.completed ? "PASS" : "FAIL")
              << "\n";
}

}  // namespace

int main() {
    const auto root = std::filesystem::current_path() / "feature-physical-matrix";
    std::filesystem::create_directories(root);
    for (const auto& spec : kKinds) {
        const auto base = featureShape(spec.kind, 0.0, 0.0);
        runCase(root, spec, "SAME_FEATURE_SAME_POSITION", base,
                featureShape(spec.kind, 0.0, 0.0));
        runCase(root, spec, "FEATURE_MOVED", base,
                featureShape(spec.kind, 4.0, 0.0));
        runCase(root, spec, "DIMENSION_CHANGED", base,
                featureShape(spec.kind, 0.0, 1.5));
        runCase(root, spec, "WHOLE_PART_MOVED", base, moved(base, 7.0));
        runCase(root, spec, "FEATURE_LOCAL_MOVED_AFTER_ALIGNMENT", base,
                moved(featureShape(spec.kind, 3.0, 0.0), 7.0));
        runCase(root, spec, "NEGATIVE_OR_AMBIGUOUS", base,
                negativeShape(spec.kind));
    }
    physicalFeatureCancellation(root);
    if (failures != 0) {
        std::cerr << failures << " physical feature matrix assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "FEATURE_PHYSICAL_MATRIX=PASS CASES=42 ROOT="
              << root.string() << '\n';
    return EXIT_SUCCESS;
}
