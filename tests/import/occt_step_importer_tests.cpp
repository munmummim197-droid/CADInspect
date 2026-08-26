#include "stepcompare/import/occt_step_importer.hpp"

#include <BRep_Builder.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <NCollection_Sequence.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_StepModelType.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Compound.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

using stepcompare::import::ImportDiagnosticCode;
using stepcompare::import::OcctStepImporter;
using stepcompare::import::StepImportRequest;

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

bool nearlyEqual(double lhs, double rhs, double tolerance = 1.0e-6) {
    return std::abs(lhs - rhs) <= tolerance;
}

std::string utf8(const std::u8string& value) {
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

std::filesystem::path fixtureDirectory() {
    return std::filesystem::current_path() /
           std::filesystem::path(u8"Dự án") /
           std::filesystem::path(u8"Bản vẽ");
}

bool writeAssemblyFixture(const std::filesystem::path& path) {
    Handle(TDocStd_Document) document;
    XCAFApp_Application::GetApplication()->NewDocument("BinXCAF", document);
    const Handle(XCAFDoc_ShapeTool) shapeTool =
        XCAFDoc_DocumentTool::ShapeTool(document->Main());

    const auto box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    gp_Trsf placement;
    placement.SetTranslation(gp_Vec(5.0, -2.0, 3.0));
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    builder.Add(compound, box.Located(TopLoc_Location(placement)));

    const TDF_Label assembly = shapeTool->AddShape(compound, true);
    TDataStd_Name::Set(assembly, u"Cụm chính");
    NCollection_Sequence<TDF_Label> components;
    if (!shapeTool->GetComponents(assembly, components, false) ||
        components.Length() != 1) {
        return false;
    }
    const TDF_Label instance = components.Value(1);
    TDF_Label part;
    if (!XCAFDoc_ShapeTool::GetReferredShape(instance, part)) {
        return false;
    }
    TDataStd_Name::Set(part, u"Chi tiết mẫu");
    TDataStd_Name::Set(instance, u"Chi tiết đặt");

    STEPCAFControl_Writer writer;
    if (!writer.Transfer(document, STEPControl_AsIs)) {
        return false;
    }
    std::ofstream stream(path, std::ios::binary);
    return stream && writer.WriteStream(stream) == IFSelect_RetDone;
}

const stepcompare::import::AssemblyNode* findNodeByName(
    const stepcompare::import::ImportedModel& model,
    const std::string& name) {
    for (const auto& node : model.nodes) {
        if (node.nameUtf8 == name) {
            return &node;
        }
    }
    return nullptr;
}

int runSuccessfulImportTest() {
    const auto directory = fixtureDirectory();
    std::filesystem::create_directories(directory);
    const auto path = directory / std::filesystem::path(u8"Chi tiết 01.step");
    if (!check(writeAssemblyFixture(path), "cannot create STEP assembly fixture")) {
        return EXIT_FAILURE;
    }

    OcctStepImporter importer;
    const auto result = importer.importStep(
        StepImportRequest{path.u8string()});
    for (const auto& diagnostic : result.diagnostics) {
        std::cerr << "IMPORT DIAGNOSTIC "
                  << static_cast<int>(diagnostic.code) << ": "
                  << diagnostic.messageUtf8 << '\n';
    }

    bool passed = check(result.succeeded(), "Unicode STEP import must succeed");
    passed &= check(result.model.lengthUnit.status ==
                        stepcompare::import::UnitNormalizationStatus::
                            NormalizedToMillimetres,
                    "length units must be normalized to millimetres");
    passed &= check(!result.model.rootNodeIds.empty(),
                    "assembly root must be retained");

    const auto* assembly = findNodeByName(result.model, utf8(u8"Cụm chính"));
    const auto* instance = findNodeByName(result.model, utf8(u8"Chi tiết đặt"));
    passed &= check(assembly != nullptr && assembly->isAssembly,
                    "assembly name and type must be retained");
    passed &= check(instance != nullptr && instance->isInstance,
                    "instance name and reference must be retained");
    if (instance != nullptr) {
        passed &= check(nearlyEqual(instance->localTransform.matrix[3], 5.0) &&
                            nearlyEqual(instance->localTransform.matrix[7], -2.0) &&
                            nearlyEqual(instance->localTransform.matrix[11], 3.0),
                        "instance location must be retained in millimetres");
    }

    passed &= check(result.model.prototypes.size() >= 1,
                    "at least one geometry prototype must be retained");
    bool foundBox = false;
    for (const auto& prototype : result.model.prototypes) {
        const auto& stats = prototype.statistics;
        if (nearlyEqual(stats.volumeMm3, 6000.0) &&
            nearlyEqual(stats.surfaceAreaMm2, 2200.0)) {
            foundBox = true;
            passed &= check(nearlyEqual(stats.boundingBox.maximum.x -
                                            stats.boundingBox.minimum.x,
                                        10.0) &&
                                nearlyEqual(stats.boundingBox.maximum.y -
                                                stats.boundingBox.minimum.y,
                                            20.0) &&
                                nearlyEqual(stats.boundingBox.maximum.z -
                                                stats.boundingBox.minimum.z,
                                            30.0),
                            "bounding box dimensions must be captured");
            passed &= check(nearlyEqual(stats.centerOfMassMm.x, 5.0) &&
                                nearlyEqual(stats.centerOfMassMm.y, 10.0) &&
                                nearlyEqual(stats.centerOfMassMm.z, 15.0),
                            "center of mass must be captured");
            passed &= check(stats.topology.solids == 1 &&
                                stats.topology.shells == 1 &&
                                stats.topology.faces == 6 &&
                                stats.topology.edges == 12 &&
                                stats.topology.vertices == 8,
                            "topology counts must be captured");
            passed &= check(stats.principalInertia.moments[0] > 0.0 &&
                                stats.principalInertia.moments[1] > 0.0 &&
                                stats.principalInertia.moments[2] > 0.0,
                            "principal moments must be captured");
        }
    }
    passed &= check(foundBox, "box geometry statistics were not found");

    std::filesystem::remove_all(std::filesystem::current_path() /
                                std::filesystem::path(u8"Dự án"));
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

int runFailureBoundaryTest() {
    OcctStepImporter importer;
    const auto result = importer.importStep(
        StepImportRequest{u8"Z:/không-tồn-tại/không-có.step"});
    bool passed = check(!result.succeeded(),
                        "missing file import must fail closed");
    bool foundExpectedDiagnostic = false;
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.code == ImportDiagnosticCode::FileOpenFailed) {
            foundExpectedDiagnostic = true;
        }
    }
    passed &= check(foundExpectedDiagnostic,
                    "missing file must return a stable diagnostic");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace

int main() {
    if (runSuccessfulImportTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return runFailureBoundaryTest();
}
