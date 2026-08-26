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

#include <cstddef>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t kMaximumOccurrenceCount = 100'000;

[[nodiscard]] bool parseCount(std::wstring_view text,
                              std::size_t& value) noexcept {
    if (text.empty()) {
        return false;
    }
    wchar_t* end{};
    const auto parsed = std::wcstoull(text.data(), &end, 10);
    if (end != text.data() + text.size() || parsed == 0U ||
        parsed > kMaximumOccurrenceCount ||
        parsed > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    value = static_cast<std::size_t>(parsed);
    return true;
}

[[nodiscard]] std::filesystem::path pathFromWide(std::wstring_view value) {
    return std::filesystem::path(value);
}

[[nodiscard]] bool writeFixture(const std::filesystem::path& path,
                                std::size_t occurrenceCount) {
    Handle(TDocStd_Document) document;
    XCAFApp_Application::GetApplication()->NewDocument("BinXCAF", document);
    const Handle(XCAFDoc_ShapeTool) shapeTool =
        XCAFDoc_DocumentTool::ShapeTool(document->Main());

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    const auto shape = BRepPrimAPI_MakeBox(10.0, 17.0, 29.0).Shape();

    constexpr std::size_t columns = 100U;
    constexpr std::size_t rows = 100U;
    for (std::size_t index = 0; index < occurrenceCount; ++index) {
        const auto column = index % columns;
        const auto row = (index / columns) % rows;
        const auto layer = index / (columns * rows);
        gp_Trsf placement;
        placement.SetTranslation(gp_Vec(
            static_cast<double>(column) * 14.0,
            static_cast<double>(row) * 21.0,
            static_cast<double>(layer) * 34.0));
        builder.Add(compound, shape.Located(TopLoc_Location(placement)));
    }
    const TDF_Label assembly = shapeTool->AddShape(compound, true);
    TDataStd_Name::Set(assembly, u"Cụm benchmark vật lý");
    NCollection_Sequence<TDF_Label> components;
    if (!shapeTool->GetComponents(assembly, components, false) ||
        components.Length() != static_cast<int>(occurrenceCount)) {
        return false;
    }
    for (int index = 1; index <= components.Length(); ++index) {
        const auto name = std::string("Benchmark occurrence ") +
                          std::to_string(index);
        TDataStd_Name::Set(components.Value(index), name.c_str());
    }

    STEPCAFControl_Writer writer;
    if (!writer.Transfer(document, STEPControl_AsIs)) {
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    return output && writer.WriteStream(output) == IFSelect_RetDone;
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: stepcompare_fixture_generator "
                     "OUTPUT_A.step OUTPUT_B.step OCCURRENCE_COUNT\n";
        return 2;
    }

    std::size_t occurrenceCount{};
    if (!parseCount(argv[3], occurrenceCount)) {
        std::cerr << "Occurrence count must be in [1, 100000].\n";
        return 2;
    }

    const auto outputA = pathFromWide(argv[1]);
    const auto outputB = pathFromWide(argv[2]);
    std::error_code error;
    std::filesystem::create_directories(outputA.parent_path(), error);
    if (error) {
        std::cerr << "Cannot create fixture directory.\n";
        return 3;
    }

    if (!writeFixture(outputA, occurrenceCount)) {
        std::cerr << "Cannot write physical XCAF STEP fixture A.\n";
        return 4;
    }
    std::filesystem::copy_file(
        outputA, outputB, std::filesystem::copy_options::overwrite_existing,
        error);
    if (error) {
        std::cerr << "Cannot create byte-identical fixture B.\n";
        return 5;
    }

    std::cout << "Generated " << occurrenceCount
              << " physical XCAF occurrences in each STEP fixture.\n";
    return 0;
}
