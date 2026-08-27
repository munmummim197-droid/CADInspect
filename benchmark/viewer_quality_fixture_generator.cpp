#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <NCollection_Sequence.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_StepModelType.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct NamedShape final {
    std::string name;
    TopoDS_Shape shape;
};

TopoDS_Shape cylinder(const double x,
                      const double y,
                      const double z,
                      const double radius,
                      const double depth) {
    return BRepPrimAPI_MakeCylinder(
               gp_Ax2(gp_Pnt(x, y, z), gp_Dir(0, 0, 1)), radius, depth)
        .Shape();
}

TopoDS_Shape holeSlotPocketPart() {
    TopoDS_Shape part = BRepPrimAPI_MakeBox(120.0, 80.0, 24.0).Shape();
    part = BRepAlgoAPI_Cut(part, cylinder(22, 22, -1, 6, 26)).Shape();

    TopoDS_Shape counterbore = cylinder(52, 22, -1, 5, 26);
    counterbore = BRepAlgoAPI_Fuse(
                      counterbore, cylinder(52, 22, 18, 10, 7))
                      .Shape();
    part = BRepAlgoAPI_Cut(part, counterbore).Shape();

    TopoDS_Shape slot = cylinder(76, 55, -1, 6, 26);
    slot = BRepAlgoAPI_Fuse(slot, cylinder(101, 55, -1, 6, 26)).Shape();
    slot = BRepAlgoAPI_Fuse(
               slot, BRepPrimAPI_MakeBox(gp_Pnt(76, 49, -1), 25, 12, 26).Shape())
               .Shape();
    part = BRepAlgoAPI_Cut(part, slot).Shape();

    const auto blindPocket =
        BRepPrimAPI_MakeBox(gp_Pnt(70, 12, 14), 35, 20, 11).Shape();
    part = BRepAlgoAPI_Cut(part, blindPocket).Shape();

    const auto rib =
        BRepPrimAPI_MakeBox(gp_Pnt(8, 48, 24), 45, 8, 14).Shape();
    return BRepAlgoAPI_Fuse(part, rib).Shape();
}

TopoDS_Shape filletChamferPart() {
    const TopoDS_Shape base = BRepPrimAPI_MakeBox(90.0, 65.0, 28.0).Shape();
    BRepFilletAPI_MakeFillet fillet(base);
    TopExp_Explorer edge(base, TopAbs_EDGE);
    if (edge.More()) {
        fillet.Add(5.0, TopoDS::Edge(edge.Current()));
    }
    TopoDS_Shape part = fillet.Shape();

    TopoDS_Shape chamferTool = cylinder(45, 32.5, -1, 6, 30);
    chamferTool = BRepAlgoAPI_Fuse(
                      chamferTool,
                      BRepPrimAPI_MakeCone(
                          gp_Ax2(gp_Pnt(45, 32.5, 22), gp_Dir(0, 0, 1)),
                          6.0, 12.0, 6.0)
                          .Shape())
                      .Shape();
    return BRepAlgoAPI_Cut(part, chamferTool).Shape();
}

TopoDS_Shape located(const TopoDS_Shape& shape,
                     const double x,
                     const double y,
                     const double z) {
    gp_Trsf transform;
    transform.SetTranslation(gp_Vec(x, y, z));
    return shape.Located(TopLoc_Location(transform));
}

TopoDS_Shape locatedRotatedZ(const TopoDS_Shape& shape,
                             const double x,
                             const double y,
                             const double z,
                             const double angleRadians) {
    gp_Trsf transform;
    transform.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)),
                          angleRadians);
    transform.SetTranslationPart(gp_Vec(x, y, z));
    return shape.Located(TopLoc_Location(transform));
}

bool writeXcaf(const std::filesystem::path& path,
               const std::string& assemblyName,
               const std::vector<NamedShape>& shapes) {
    Handle(TDocStd_Document) document;
    XCAFApp_Application::GetApplication()->NewDocument("BinXCAF", document);
    const Handle(XCAFDoc_ShapeTool) shapeTool =
        XCAFDoc_DocumentTool::ShapeTool(document->Main());

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const auto& item : shapes) {
        builder.Add(compound, item.shape);
    }
    const TDF_Label assembly = shapeTool->AddShape(compound, true);
    TDataStd_Name::Set(assembly, assemblyName.c_str());
    NCollection_Sequence<TDF_Label> components;
    if (shapeTool->GetComponents(assembly, components, false)) {
        for (int index = 1; index <= components.Length() &&
                            index <= static_cast<int>(shapes.size());
             ++index) {
            TDataStd_Name::Set(components.Value(index),
                               shapes[static_cast<std::size_t>(index - 1)].name.c_str());
        }
    }

    STEPCAFControl_Writer writer;
    if (!writer.Transfer(document, STEPControl_AsIs)) {
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    return output && writer.WriteStream(output) == IFSelect_RetDone;
}

bool writePair(const std::filesystem::path& directory,
               const std::string& stem,
               const std::string& assemblyName,
               const std::vector<NamedShape>& shapes) {
    const auto a = directory / (stem + "-A.step");
    const auto b = directory / (stem + "-B.step");
    if (!writeXcaf(a, assemblyName, shapes)) {
        return false;
    }
    std::error_code error;
    std::filesystem::copy_file(
        a, b, std::filesystem::copy_options::overwrite_existing, error);
    return !error;
}

bool writeChangedAssemblyPair(const std::filesystem::path& directory,
                              const TopoDS_Shape& featured,
                              const TopoDS_Shape& finished) {
    const auto geometryChanged =
        BRepAlgoAPI_Cut(finished, cylinder(20, 20, -1, 4, 32)).Shape();
    const std::vector<NamedShape> aShapes = {
        {"Repeated feature plate", located(featured, 0, 0, 0)},
        {"Geometry-change candidate", located(finished, 145, 0, 0)},
        {"Repeated feature plate", located(featured, 0, 115, 15)},
        {"Stable reference", located(finished, 145, 115, 15)}};
    const std::vector<NamedShape> bShapes = {
        {"Repeated feature plate", located(featured, 8, 0, 0)},
        {"Geometry-change candidate", located(geometryChanged, 145, 0, 0)},
        {"Repeated feature plate",
         locatedRotatedZ(featured, 0, 115, 15, 0.20943951023931953)},
        {"Stable reference", located(finished, 145, 115, 15)}};
    return writeXcaf(directory / "occurrence-change-assembly-A.step",
                     "Occurrence identity physical assembly", aShapes) &&
           writeXcaf(directory / "occurrence-change-assembly-B.step",
                     "Occurrence identity physical assembly", bShapes);
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: stepcompare_viewer_quality_fixture_generator OUTPUT_DIR\n";
        return 2;
    }
    const std::filesystem::path output(argv[1]);
    std::error_code error;
    std::filesystem::create_directories(output, error);
    if (error) {
        std::cerr << "Cannot create output directory.\n";
        return 3;
    }

    const auto featured = holeSlotPocketPart();
    const auto finished = filletChamferPart();
    if (!writePair(output, "hole-slot-pocket", "Hole slot pocket",
                   {{"Feature-rich plate", featured}}) ||
        !writePair(output, "fillet-chamfer", "Fillet chamfer",
                   {{"Fillet and chamfer part", finished}}) ||
        !writePair(output, "complex-assembly", "Complex feature assembly",
                   {{"Feature plate 1", located(featured, 0, 0, 0)},
                    {"Fillet chamfer 1", located(finished, 145, 0, 0)},
                    {"Feature plate 2", located(featured, 0, 115, 15)},
                    {"Fillet chamfer 2", located(finished, 145, 115, 15)}}) ||
        !writeChangedAssemblyPair(output, featured, finished)) {
        std::cerr << "Cannot write physical viewer quality fixtures.\n";
        return 4;
    }
    std::cout << "Generated physical hole/slot/pocket, fillet/chamfer, "
                 "complex assembly, and occurrence-change STEP fixture pairs.\n";
    return 0;
}
