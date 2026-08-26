#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <NCollection_Sequence.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);

    const auto shape = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    if (!check(!shape.IsNull(), "OCCT box construction failed")) {
        return EXIT_FAILURE;
    }

    GProp_GProps volumeProperties;
    BRepGProp::VolumeProperties(shape, volumeProperties);
    if (!check(std::abs(volumeProperties.Mass() - 6000.0) < 1.0e-7,
               "OCCT volume calculation is outside tolerance")) {
        return EXIT_FAILURE;
    }

    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    if (!check(!bounds.IsVoid(), "OCCT bounding box is void")) {
        return EXIT_FAILURE;
    }

    BRepMesh_IncrementalMesh mesher(shape, 0.25, false, 0.5, true);
    if (!check(mesher.IsDone(), "OCCT triangulation did not complete")) {
        return EXIT_FAILURE;
    }

    const QString unicodeDirectory =
        QDir::current().filePath(QString::fromUtf8(u8"Dự án/Bản vẽ"));
    if (!check(QDir().mkpath(unicodeDirectory),
               "Qt failed to create the Unicode fixture path")) {
        return EXIT_FAILURE;
    }
    const QString stepPath = QDir(unicodeDirectory).filePath(
        QString::fromUtf8(u8"Chi tiết 01.step"));
    const QByteArray encodedPath = stepPath.toUtf8();

    STEPControl_Writer writer;
    if (!check(writer.Transfer(shape, STEPControl_AsIs) == IFSelect_RetDone,
               "STEP transfer to writer failed")) {
        return EXIT_FAILURE;
    }
    if (!check(writer.Write(encodedPath.constData()) == IFSelect_RetDone,
               "STEP write to Unicode path failed")) {
        return EXIT_FAILURE;
    }
    if (!check(QFileInfo::exists(stepPath),
               "STEP fixture does not exist at the Unicode path")) {
        return EXIT_FAILURE;
    }

    const Handle(TDocStd_Document) document = new TDocStd_Document("BinXCAF");
    STEPCAFControl_Reader reader;
    if (!check(reader.ReadFile(encodedPath.constData()) == IFSelect_RetDone,
               "STEP XCAF read from Unicode path failed")) {
        return EXIT_FAILURE;
    }
    if (!check(reader.Transfer(document), "STEP XCAF transfer failed")) {
        return EXIT_FAILURE;
    }

    const Handle(XCAFDoc_ShapeTool) shapeTool =
        XCAFDoc_DocumentTool::ShapeTool(document->Main());
    NCollection_Sequence<TDF_Label> roots;
    shapeTool->GetFreeShapes(roots);
    if (!check(roots.Length() == 1,
               "XCAF document must contain exactly one free shape")) {
        return EXIT_FAILURE;
    }

    if (qEnvironmentVariableIsEmpty("STEPCOMPARE_KEEP_FIXTURE")) {
        QDir(QDir::current().filePath(QString::fromUtf8(u8"Dự án")))
            .removeRecursively();
    } else {
        std::cout << "Fixture retained at " << encodedPath.constData() << '\n';
    }
    std::cout << "Qt/OCCT STEP, XCAF, meshing, and Unicode smoke passed\n";
    return EXIT_SUCCESS;
}
