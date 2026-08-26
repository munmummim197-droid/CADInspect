#include "stepcompare/deep/occt_deep_geometry_engine.hpp"
#include "stepcompare/import/occt_step_importer.hpp"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>

namespace {

using stepcompare::deep::DeepGeometryRequest;
using stepcompare::deep::DeepGeometryStatus;
using stepcompare::deep::OcctDeepGeometryEngine;
using stepcompare::import::GeometryPayloadPtr;
using stepcompare::import::OcctStepImporter;
using stepcompare::import::StepImportRequest;

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

bool writeStep(const std::filesystem::path& path, const TopoDS_Shape& shape) {
    STEPControl_Writer writer;
    if (writer.Transfer(shape, STEPControl_AsIs) != IFSelect_RetDone) {
        return false;
    }
    std::ofstream stream(path, std::ios::binary);
    return stream && writer.WriteStream(stream) == IFSelect_RetDone;
}

GeometryPayloadPtr importSinglePrototype(const std::filesystem::path& path) {
    OcctStepImporter importer;
    const auto result = importer.importStep(StepImportRequest{path.u8string()});
    if (!result.succeeded() || result.model.prototypes.size() != 1) {
        for (const auto& diagnostic : result.diagnostics) {
            std::cerr << "IMPORT: " << diagnostic.messageUtf8 << '\n';
        }
        return {};
    }
    return result.model.prototypes.front().geometry;
}

TopoDS_Shape transformed(const TopoDS_Shape& shape,
                         double angleDegrees,
                         const gp_Vec& translation) {
    gp_Trsf rotation;
    rotation.SetRotation(gp_Ax1(gp_Pnt(0.0, 0.0, 0.0),
                                gp_Dir(0.0, 0.0, 1.0)),
                         angleDegrees * std::numbers::pi / 180.0);
    gp_Trsf move;
    move.SetTranslation(translation);
    return BRepBuilderAPI_Transform(shape, move * rotation, true).Shape();
}

bool mapsPoint(const stepcompare::import::RigidTransformMm& transform,
               const gp_Pnt& from,
               const gp_Pnt& expected,
               double tolerance) {
    const auto& m = transform.matrix;
    const double x = m[0] * from.X() + m[1] * from.Y() +
                     m[2] * from.Z() + m[3];
    const double y = m[4] * from.X() + m[5] * from.Y() +
                     m[6] * from.Z() + m[7];
    const double z = m[8] * from.X() + m[9] * from.Y() +
                     m[10] * from.Z() + m[11];
    return std::abs(x - expected.X()) <= tolerance &&
           std::abs(y - expected.Y()) <= tolerance &&
           std::abs(z - expected.Z()) <= tolerance;
}

bool compareSupportedRigidCases(const std::filesystem::path& directory) {
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    const auto pathA = directory / "box-a.step";
    const auto pathTranslated = directory / "box-translated.step";
    const auto pathRotated = directory / "box-rotated.step";
    if (!writeStep(pathA, box) ||
        !writeStep(pathTranslated,
                   transformed(box, 0.0, gp_Vec(5.0, -2.0, 3.0))) ||
        !writeStep(pathRotated,
                   transformed(box, 31.0, gp_Vec(40.0, -10.0, 7.0)))) {
        return check(false, "cannot write rigid-alignment STEP fixtures");
    }

    const auto geometryA = importSinglePrototype(pathA);
    const auto geometryTranslated = importSinglePrototype(pathTranslated);
    const auto geometryRotated = importSinglePrototype(pathRotated);
    if (!geometryA || !geometryTranslated || !geometryRotated) {
        return check(false, "cannot import rigid-alignment STEP fixtures");
    }

    OcctDeepGeometryEngine engine;
    const auto translatedResult = engine.compareAligned(
        DeepGeometryRequest{geometryA, geometryTranslated});
    bool passed = check(translatedResult.status ==
                            DeepGeometryStatus::SameGeometry,
                        "translated box must be proven same after alignment");
    passed &= check(translatedResult.alignmentProven,
                    "translated box alignment must be proven");
    passed &= check(translatedResult.symmetricDifferenceVolumeMm3 <= 1.0e-4,
                    "translated box Vdiff must be near zero");
    passed &= check(mapsPoint(translatedResult.transformBToA,
                              gp_Pnt(10.0, 8.0, 18.0),
                              gp_Pnt(5.0, 10.0, 15.0),
                              1.0e-5),
                    "translated B->A transform must map its COM");

    const auto rotatedResult = engine.compareAligned(
        DeepGeometryRequest{geometryA, geometryRotated});
    passed &= check(rotatedResult.status == DeepGeometryStatus::SameGeometry,
                    "rotated box must be proven same after alignment");
    passed &= check(rotatedResult.alignmentProven,
                    "rotated box alignment must be proven");
    passed &= check(rotatedResult.symmetricDifferenceVolumeMm3 <= 1.0e-4,
                    "rotated box Vdiff must be near zero");

    const gp_Pnt originalCom(5.0, 10.0, 15.0);
    gp_Pnt rotatedCom = originalCom;
    gp_Trsf forward;
    forward.SetRotation(gp_Ax1(gp_Pnt(0.0, 0.0, 0.0),
                               gp_Dir(0.0, 0.0, 1.0)),
                        31.0 * std::numbers::pi / 180.0);
    rotatedCom.Transform(forward);
    rotatedCom.Translate(gp_Vec(40.0, -10.0, 7.0));
    passed &= check(mapsPoint(rotatedResult.transformBToA,
                              rotatedCom,
                              originalCom,
                              1.0e-5),
                    "rotated B->A transform must map its COM");
    return passed;
}

bool compareChangedGeometry(const std::filesystem::path& directory) {
    const auto pathA = directory / "changed-a.step";
    const auto pathB = directory / "changed-b.step";
    if (!writeStep(pathA, BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape()) ||
        !writeStep(pathB, BRepPrimAPI_MakeBox(11.0, 20.0, 30.0).Shape())) {
        return check(false, "cannot write changed-geometry fixtures");
    }
    OcctDeepGeometryEngine engine;
    const auto result = engine.compareAligned({importSinglePrototype(pathA),
                                                importSinglePrototype(pathB)});
    return check(result.status == DeepGeometryStatus::GeometryChanged &&
                     result.symmetricDifferenceVolumeMm3 > 1.0,
                 "dimension change must produce non-zero Vdiff");
}

bool rejectAmbiguousAndOpenShell(const std::filesystem::path& directory) {
    const auto cubeA = directory / "cube-a.step";
    const auto cubeB = directory / "cube-b.step";
    const auto shell = directory / "open-shell.step";
    const TopoDS_Shape cube = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
    const TopoDS_Shape face =
        BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0.0, 0.0, 0.0),
                                      gp_Dir(0.0, 0.0, 1.0)),
                                0.0,
                                10.0,
                                0.0,
                                10.0)
            .Shape();
    if (!writeStep(cubeA, cube) ||
        !writeStep(cubeB, transformed(cube, 17.0, gp_Vec(3.0, 4.0, 5.0))) ||
        !writeStep(shell, face)) {
        return check(false, "cannot write fail-closed fixtures");
    }

    OcctDeepGeometryEngine engine;
    const auto ambiguous = engine.compareAligned(
        {importSinglePrototype(cubeA), importSinglePrototype(cubeB)});
    bool passed = check(ambiguous.status ==
                            DeepGeometryStatus::AlignmentNotProven,
                        "symmetric cube alignment must fail closed");

    const auto openShellGeometry = importSinglePrototype(shell);
    const auto openShell = engine.compareAligned(
        {openShellGeometry, openShellGeometry});
    passed &= check(openShell.status ==
                        DeepGeometryStatus::OpenShellUnsupported,
                    "open shell deep solid check must be graceful");
    return passed;
}

}  // namespace

int main() {
    const auto directory = std::filesystem::current_path() / "deep-fixtures";
    std::filesystem::create_directories(directory);
    const bool passed = compareSupportedRigidCases(directory) &&
                        compareChangedGeometry(directory) &&
                        rejectAmbiguousAndOpenShell(directory);
    std::filesystem::remove_all(directory);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
