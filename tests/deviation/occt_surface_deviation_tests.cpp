#include <stepcompare/deviation/occt_surface_deviation_engine.hpp>

#include "adapters/occt/occt_geometry_payload.hpp"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

stepcompare::import::GeometryPayloadPtr payload(const TopoDS_Shape& shape) {
    return std::make_shared<
        stepcompare::adapters::occt::OcctGeometryPayload>(shape);
}

stepcompare::import::RigidTransformMm translation(double x, double y,
                                                  double z) {
    stepcompare::import::RigidTransformMm value{};
    value.matrix[3] = x;
    value.matrix[7] = y;
    value.matrix[11] = z;
    return value;
}

TopoDS_Shape translated(const TopoDS_Shape& shape, double x, double y,
                        double z) {
    gp_Trsf transform;
    transform.SetTranslation(gp_Vec(x, y, z));
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
}

void identicalAndBvhTests() {
    using namespace stepcompare::deviation;
    const auto box = payload(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape());
    OcctSurfaceDeviationEngine engine;
    const auto result = engine.compare({box, box});
    expect(result.status == SurfaceDeviationStatus::WithinTolerance,
           "identical geometry must be within tolerance");
    expect(result.maximumMm < 1.0e-9 && result.meanMm < 1.0e-9 &&
               result.rmsMm < 1.0e-9 && result.percentileMm < 1.0e-9,
           "identical geometry deviation statistics must be near zero");
    expect(result.samplesAToB > 0 && result.samplesBToA > 0,
           "comparison must sample both directions");
    const auto exhaustivePairs =
        result.samplesAToB * result.trianglesB +
        result.samplesBToA * result.trianglesA;
    expect(result.triangleDistanceEvaluations > 0 &&
               result.triangleDistanceEvaluations < exhaustivePairs,
           "nearest queries must use BVH pruning, not every triangle pair");

    SurfaceDeviationRequest capped{box, box};
    capped.options.maximumSamplesPerDirection = 7;
    const auto cappedResult = engine.compare(capped);
    expect(cappedResult.samplesAToB == 7 && cappedResult.samplesBToA == 7,
           "sampling budget must cap each direction deterministically");
}

void alignmentAndChangedDimensionTests() {
    using namespace stepcompare::deviation;
    const TopoDS_Shape original = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    const auto geometryA = payload(original);
    const auto geometryTranslated = payload(translated(original, 5.0, -2.0, 3.0));
    OcctSurfaceDeviationEngine engine;

    SurfaceDeviationRequest aligned{geometryA, geometryTranslated};
    aligned.transformBToA = translation(-5.0, 2.0, -3.0);
    const auto alignedResult = engine.compare(aligned);
    expect(alignedResult.status == SurfaceDeviationStatus::WithinTolerance &&
               alignedResult.maximumMm < 1.0e-8,
           "provided B-to-A alignment must remove rigid translation");

    const auto unaligned = engine.compare({geometryA, geometryTranslated});
    expect(unaligned.status == SurfaceDeviationStatus::DeviationFound &&
               unaligned.maximumMm > 1.0,
           "absolute translated geometry must show surface deviation");

    const auto changed = engine.compare(
        {geometryA, payload(BRepPrimAPI_MakeBox(11.0, 20.0, 30.0).Shape())});
    expect(changed.status == SurfaceDeviationStatus::DeviationFound &&
               changed.maximumMm > 0.5,
           "dimension change must produce measurable bidirectional deviation");
}

void openShellAndCancellationTests() {
    using namespace stepcompare::deviation;
    const TopoDS_Shape face =
        BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0.0, 0.0, 0.0),
                                      gp_Dir(0.0, 0.0, 1.0)),
                                0.0, 10.0, 0.0, 10.0)
            .Shape();
    const auto shell = payload(face);
    OcctSurfaceDeviationEngine engine;
    const auto openShell = engine.compare({shell, shell});
    expect(openShell.status == SurfaceDeviationStatus::WithinTolerance,
           "open shell surface comparison must complete gracefully");

    SurfaceDeviationRequest cancelled{shell, shell};
    cancelled.isCancelled = [] { return true; };
    const auto cancelledResult = engine.compare(cancelled);
    expect(cancelledResult.status == SurfaceDeviationStatus::Cancelled,
           "pre-cancelled work must return CANCELLED fail-closed");
    expect(!cancelledResult.diagnostics.empty() &&
               cancelledResult.diagnostics.front().code ==
                   SurfaceDeviationDiagnosticCode::Cancelled,
           "cancellation must carry an explicit diagnostic");
}

void invalidOptionsTests() {
    using namespace stepcompare::deviation;
    const auto box = payload(BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape());
    SurfaceDeviationRequest request{box, box};
    request.options.meshDeflectionMm = 0.0;
    OcctSurfaceDeviationEngine engine;
    const auto result = engine.compare(request);
    expect(result.status == SurfaceDeviationStatus::Error &&
               !result.diagnostics.empty() &&
               result.diagnostics.front().code ==
                   SurfaceDeviationDiagnosticCode::InvalidOptions,
           "invalid meshing options must fail closed");
}

}  // namespace

int main() {
    identicalAndBvhTests();
    alignmentAndChangedDimensionTests();
    openShellAndCancellationTests();
    invalidOptionsTests();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All surface deviation tests passed\n";
    return EXIT_SUCCESS;
}
