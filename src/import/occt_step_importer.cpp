#include "stepcompare/import/occt_step_importer.hpp"

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GProp_PrincipalProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <NCollection_Sequence.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <Standard_Failure.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_Label.hxx>
#include <TDF_Tool.hxx>
#include <TDocStd_Document.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopLoc_Location.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Shape.hxx>
#include <UnitsMethods_LengthUnit.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace stepcompare::import {
namespace {

constexpr double kMillimetresPerInternalUnit = 1.0;
constexpr std::size_t kMaximumAssemblyDepth = 1024;

std::mutex& occtImportMutex() {
    static std::mutex mutex;
    return mutex;
}

class OcctGeometryPayload final : public GeometryPayload {
public:
    explicit OcctGeometryPayload(TopoDS_Shape shape)
        : shape_(std::move(shape)) {}

private:
    // This class is intentionally private to this translation unit so OCCT
    // types never become part of the core-facing import contract.
    TopoDS_Shape shape_;
};

std::string bytesFromUtf8(const std::u8string& value) {
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

std::string utf8FromExtendedString(const TCollection_ExtendedString& value) {
    std::vector<char> buffer(
        static_cast<std::size_t>(value.LengthOfCString()) + 1U, '\0');
    Standard_PCharacter destination = buffer.data();
    value.ToUTF8CString(destination);
    return std::string(buffer.data());
}

std::string labelId(const TDF_Label& label) {
    TCollection_AsciiString entry;
    TDF_Tool::Entry(label, entry);
    return entry.ToCString();
}

std::string labelNameUtf8(const TDF_Label& label) {
    Handle(TDataStd_Name) name;
    if (label.FindAttribute(TDataStd_Name::GetID(), name) && !name.IsNull()) {
        return utf8FromExtendedString(name->Get());
    }
    return {};
}

domain::Vec3Mm toVec3Mm(const gp_Pnt& point) noexcept {
    return {point.X(), point.Y(), point.Z()};
}

domain::UnitDirection toDirection(const gp_Vec& vector) noexcept {
    return {vector.X(), vector.Y(), vector.Z()};
}

RigidTransformMm toTransform(const TopLoc_Location& location) {
    RigidTransformMm result;
    const gp_Trsf& transform = location.Transformation();
    for (int row = 1; row <= 3; ++row) {
        for (int column = 1; column <= 4; ++column) {
            result.matrix[static_cast<std::size_t>((row - 1) * 4 +
                                                   (column - 1))] =
                transform.Value(row, column);
        }
    }
    return result;
}

std::uint64_t countTopology(const TopoDS_Shape& shape,
                            TopAbs_ShapeEnum type) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> entities;
    TopExp::MapShapes(shape, type, entities);
    return static_cast<std::uint64_t>(entities.Extent());
}

domain::PrincipalInertia principalInertia(const GProp_GProps& properties) {
    const GProp_PrincipalProps principal = properties.PrincipalProperties();
    domain::PrincipalInertia result;
    principal.Moments(result.moments[0], result.moments[1], result.moments[2]);
    result.axes = {
        toDirection(principal.FirstAxisOfInertia()),
        toDirection(principal.SecondAxisOfInertia()),
        toDirection(principal.ThirdAxisOfInertia()),
    };
    return result;
}

domain::GeometryStatistics analyzeGeometry(const TopoDS_Shape& shape) {
    if (shape.IsNull()) {
        throw std::runtime_error("XCAF prototype contains a null shape");
    }

    domain::GeometryStatistics statistics;
    statistics.topology.solids = countTopology(shape, TopAbs_SOLID);
    statistics.topology.shells = countTopology(shape, TopAbs_SHELL);
    statistics.topology.faces = countTopology(shape, TopAbs_FACE);
    statistics.topology.edges = countTopology(shape, TopAbs_EDGE);
    statistics.topology.vertices = countTopology(shape, TopAbs_VERTEX);

    Bnd_Box bounds;
    BRepBndLib::AddOptimal(shape, bounds, false, false);
    if (bounds.IsVoid() || bounds.IsOpen()) {
        throw std::runtime_error("Cannot compute a finite geometry bounding box");
    }
    double xMin{};
    double yMin{};
    double zMin{};
    double xMax{};
    double yMax{};
    double zMax{};
    bounds.Get(xMin, yMin, zMin, xMax, yMax, zMax);
    statistics.boundingBox = {{xMin, yMin, zMin}, {xMax, yMax, zMax}};

    GProp_GProps surfaceProperties;
    if (statistics.topology.faces > 0) {
        BRepGProp::SurfaceProperties(shape, surfaceProperties, true, false);
        statistics.surfaceAreaMm2 = std::abs(surfaceProperties.Mass());
    }

    GProp_GProps massProperties;
    if (statistics.topology.solids > 0) {
        BRepGProp::VolumeProperties(shape, massProperties, true, true, false);
        statistics.volumeMm3 = std::abs(massProperties.Mass());
    }

    constexpr double kUsableMass = 1.0e-15;
    if (statistics.volumeMm3 > kUsableMass) {
        statistics.centerOfMassMm = toVec3Mm(massProperties.CentreOfMass());
        statistics.principalInertia = principalInertia(massProperties);
        statistics.closedSolidEvidence = true;
    } else if (statistics.surfaceAreaMm2 > kUsableMass) {
        statistics.centerOfMassMm = toVec3Mm(surfaceProperties.CentreOfMass());
        statistics.principalInertia = principalInertia(surfaceProperties);
        statistics.closedSolidEvidence = false;
    } else {
        throw std::runtime_error(
            "Geometry has neither usable volume nor surface properties");
    }

    return statistics;
}

class ModelBuilder final {
public:
    ModelBuilder(const Handle(XCAFDoc_ShapeTool)& shapeTool,
                 ImportedModel& model)
        : shapeTool_(shapeTool), model_(model) {}

    std::string addNode(const TDF_Label& label,
                        std::optional<std::string> parentId,
                        const std::string& occurrenceId,
                        std::size_t depth) {
        if (depth > kMaximumAssemblyDepth) {
            throw std::runtime_error("Assembly nesting exceeds safety limit");
        }

        TDF_Label definition = label;
        const bool isInstance = XCAFDoc_ShapeTool::IsReference(label);
        if (isInstance && !XCAFDoc_ShapeTool::GetReferredShape(label, definition)) {
            throw std::runtime_error("XCAF reference has no referred shape");
        }

        const bool isAssembly = XCAFDoc_ShapeTool::IsAssembly(definition);
        if (!isAssembly) {
            ensurePrototype(definition);
        }

        AssemblyNode node;
        node.id = occurrenceId;
        node.parentId = std::move(parentId);
        if (!isAssembly) {
            node.prototypeId = labelId(definition);
        }
        node.nameUtf8 = labelNameUtf8(label);
        if (node.nameUtf8.empty()) {
            node.nameUtf8 = labelNameUtf8(definition);
        }
        node.localTransform = toTransform(XCAFDoc_ShapeTool::GetLocation(label));
        node.isAssembly = isAssembly;
        node.isInstance = isInstance;

        const std::size_t nodeIndex = model_.nodes.size();
        model_.nodes.push_back(std::move(node));

        if (isAssembly) {
            NCollection_Sequence<TDF_Label> components;
            if (!shapeTool_->GetComponents(definition, components, false)) {
                throw std::runtime_error(
                    "XCAF assembly could not enumerate its components");
            }
            for (int index = 1; index <= components.Length(); ++index) {
                const TDF_Label& component = components.Value(index);
                const std::string childOccurrence =
                    occurrenceId + "/" + labelId(component);
                model_.nodes[nodeIndex].childIds.push_back(childOccurrence);
                addNode(component, occurrenceId, childOccurrence, depth + 1U);
            }
        }
        return occurrenceId;
    }

private:
    void ensurePrototype(const TDF_Label& definition) {
        const std::string id = labelId(definition);
        if (prototypeIndices_.contains(id)) {
            return;
        }

        const TopoDS_Shape shape = shapeTool_->GetShape(definition);
        PartPrototype prototype;
        prototype.id = id;
        prototype.nameUtf8 = labelNameUtf8(definition);
        prototype.statistics = analyzeGeometry(shape);
        prototype.geometry = std::make_shared<OcctGeometryPayload>(shape);
        prototypeIndices_.emplace(id, model_.prototypes.size());
        model_.prototypes.push_back(std::move(prototype));
    }

    Handle(XCAFDoc_ShapeTool) shapeTool_;
    ImportedModel& model_;
    std::unordered_map<std::string, std::size_t> prototypeIndices_;
};

void appendDiagnostic(StepImportResult& result,
                      ImportDiagnosticSeverity severity,
                      ImportDiagnosticCode code,
                      std::string message) {
    result.diagnostics.push_back({severity, code, std::move(message)});
}

void captureAndNormalizeUnits(STEPCAFControl_Reader& reader,
                              StepImportResult& result) {
    NCollection_Sequence<TCollection_AsciiString> lengthUnits;
    NCollection_Sequence<TCollection_AsciiString> angleUnits;
    NCollection_Sequence<TCollection_AsciiString> solidAngleUnits;
    STEPControl_Reader& stepReader = reader.ChangeReader();
    stepReader.FileUnits(lengthUnits, angleUnits, solidAngleUnits);
    for (int index = 1; index <= lengthUnits.Length(); ++index) {
        result.model.lengthUnit.sourceUnitNames.emplace_back(
            lengthUnits.Value(index).ToCString());
    }

    // STEPControl_Reader expresses its target system unit in millimetres.
    // Setting this before Transfer forces all transferred XCAF geometry and
    // placement translations into the core's canonical millimetre unit.
    stepReader.SetSystemLengthUnit(kMillimetresPerInternalUnit);
    const double configuredUnit = stepReader.SystemLengthUnit();
    if (!std::isfinite(configuredUnit) ||
        std::abs(configuredUnit - kMillimetresPerInternalUnit) >
            std::numeric_limits<double>::epsilon()) {
        result.model.lengthUnit.status = UnitNormalizationStatus::Unverified;
        appendDiagnostic(result,
                         ImportDiagnosticSeverity::Error,
                         ImportDiagnosticCode::UnitNormalizationUnverified,
                         "OCCT did not accept millimetres as the transfer unit");
        return;
    }

    result.model.lengthUnit.status =
        UnitNormalizationStatus::NormalizedToMillimetres;
    result.model.lengthUnit.targetMillimetresPerUnit = configuredUnit;
    if (result.model.lengthUnit.sourceUnitNames.empty()) {
        appendDiagnostic(result,
                         ImportDiagnosticSeverity::Warning,
                         ImportDiagnosticCode::UnitMetadataMissing,
                         "STEP file exposes no source length-unit metadata; "
                         "OCCT transfer output is still normalized to millimetres");
    }
}

StepImportResult importStepImpl(const StepImportRequest& request) {
    StepImportResult result;
    result.model.sourcePathUtf8 = request.sourcePathUtf8;
    if (request.sourcePathUtf8.empty()) {
        appendDiagnostic(result,
                         ImportDiagnosticSeverity::Error,
                         ImportDiagnosticCode::InvalidUtf8Path,
                         "STEP source path is empty");
        return result;
    }

    std::filesystem::path sourcePath;
    try {
        sourcePath = std::filesystem::path(request.sourcePathUtf8);
    } catch (const std::exception&) {
        appendDiagnostic(result,
                         ImportDiagnosticSeverity::Error,
                         ImportDiagnosticCode::InvalidUtf8Path,
                         "STEP source path is not valid UTF-8");
        return result;
    }

    std::ifstream stream(sourcePath, std::ios::binary);
    if (!stream) {
        appendDiagnostic(result,
                         ImportDiagnosticSeverity::Error,
                         ImportDiagnosticCode::FileOpenFailed,
                         "Cannot open STEP source file");
        return result;
    }

    STEPCAFControl_Reader reader;
    reader.SetNameMode(true);
    reader.SetColorMode(true);
    reader.SetLayerMode(true);
    reader.SetPropsMode(true);
    reader.SetGDTMode(true);
    reader.SetMatMode(true);

    const std::string sourceName = bytesFromUtf8(request.sourcePathUtf8);
    if (reader.ReadStream(sourceName.c_str(), stream) != IFSelect_RetDone) {
        appendDiagnostic(result,
                         ImportDiagnosticSeverity::Error,
                         ImportDiagnosticCode::StepReadFailed,
                         "OCCT could not parse the STEP stream");
        return result;
    }

    captureAndNormalizeUnits(reader, result);
    if (result.model.lengthUnit.status !=
        UnitNormalizationStatus::NormalizedToMillimetres) {
        return result;
    }

    Handle(TDocStd_Document) document;
    const Handle(XCAFApp_Application) application =
        XCAFApp_Application::GetApplication();
    application->NewDocument("BinXCAF", document);
    if (!reader.Transfer(document)) {
        appendDiagnostic(result,
                         ImportDiagnosticSeverity::Error,
                         ImportDiagnosticCode::StepTransferFailed,
                         "OCCT could not transfer STEP data into XCAF");
        application->Close(document);
        return result;
    }
    XCAFDoc_DocumentTool::SetLengthUnit(
        document, 1.0, UnitsMethods_LengthUnit_Millimeter);

    const Handle(XCAFDoc_ShapeTool) shapeTool =
        XCAFDoc_DocumentTool::ShapeTool(document->Main());
    NCollection_Sequence<TDF_Label> roots;
    shapeTool->GetFreeShapes(roots);
    if (roots.IsEmpty()) {
        appendDiagnostic(result,
                         ImportDiagnosticSeverity::Error,
                         ImportDiagnosticCode::EmptyDocument,
                         "STEP transfer produced no free XCAF shapes");
        application->Close(document);
        return result;
    }

    ModelBuilder builder(shapeTool, result.model);
    for (int index = 1; index <= roots.Length(); ++index) {
        const TDF_Label& root = roots.Value(index);
        const std::string rootId = labelId(root);
        result.model.rootNodeIds.push_back(rootId);
        builder.addNode(root, std::nullopt, rootId, 0U);
    }

    application->Close(document);
    return result;
}

}  // namespace

StepImportResult OcctStepImporter::importStep(
    const StepImportRequest& request) noexcept {
    // OCCT import sessions and the XCAF application singleton are conservatively
    // serialized. Downstream immutable prototypes can be analyzed in parallel.
    const std::scoped_lock lock(occtImportMutex());
    try {
        return importStepImpl(request);
    } catch (const Standard_Failure& failure) {
        StepImportResult result;
        result.model.sourcePathUtf8 = request.sourcePathUtf8;
        appendDiagnostic(result,
                         ImportDiagnosticSeverity::Error,
                         ImportDiagnosticCode::OcctFailure,
                         failure.what() != nullptr
                             ? failure.what()
                             : "OCCT raised an unspecified failure");
        return result;
    } catch (const std::exception& failure) {
        StepImportResult result;
        result.model.sourcePathUtf8 = request.sourcePathUtf8;
        appendDiagnostic(result,
                         ImportDiagnosticSeverity::Error,
                         ImportDiagnosticCode::UnexpectedFailure,
                         failure.what());
        return result;
    } catch (...) {
        StepImportResult result;
        result.model.sourcePathUtf8 = request.sourcePathUtf8;
        appendDiagnostic(result,
                         ImportDiagnosticSeverity::Error,
                         ImportDiagnosticCode::UnexpectedFailure,
                         "Unknown exception escaped the OCCT import implementation");
        return result;
    }
}

}  // namespace stepcompare::import
