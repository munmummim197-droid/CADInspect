#include "stepcompare/application/comparison_coordinator.hpp"

#include "stepcompare/reporting/writers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <stop_token>
#include <utility>

namespace {

using stepcompare::application::ComparisonCoordinator;
using stepcompare::application::ComparisonRequest;
using stepcompare::application::ComparisonRunStatus;
using stepcompare::deep::DeepGeometryRequest;
using stepcompare::deep::DeepGeometryResult;
using stepcompare::deep::DeepGeometryStatus;
using stepcompare::domain::Decision;
using stepcompare::import::ImportedModel;
using stepcompare::import::StepImportRequest;
using stepcompare::import::StepImportResult;

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class FakeGeometry final : public stepcompare::import::GeometryPayload {};

stepcompare::domain::GeometryStatistics boxStatistics(double x = 10.0) {
    stepcompare::domain::GeometryStatistics statistics;
    statistics.boundingBox = {{0.0, 0.0, 0.0}, {x, 20.0, 30.0}};
    statistics.volumeMm3 = x * 20.0 * 30.0;
    statistics.surfaceAreaMm2 = 2.0 * (x * 20.0 + x * 30.0 + 600.0);
    statistics.centerOfMassMm = {x * 0.5, 10.0, 15.0};
    statistics.topology = {1, 1, 6, 12, 8};
    statistics.principalInertia.moments = {1.0, 2.0, 3.0};
    statistics.principalInertia.axes = {{{1.0, 0.0, 0.0},
                                         {0.0, 1.0, 0.0},
                                         {0.0, 0.0, 1.0}}};
    statistics.closedSolidEvidence = true;
    return statistics;
}

ImportedModel partModel(std::u8string path,
                        double x = 10.0,
                        double translationX = 0.0) {
    ImportedModel model;
    model.sourcePathUtf8 = std::move(path);
    model.lengthUnit.status =
        stepcompare::import::UnitNormalizationStatus::NormalizedToMillimetres;
    model.rootNodeIds = {"root"};
    stepcompare::import::PartPrototype prototype;
    prototype.id = "part";
    prototype.nameUtf8 = "Part";
    prototype.statistics = boxStatistics(x);
    prototype.geometry = std::make_shared<FakeGeometry>();
    model.prototypes.push_back(std::move(prototype));
    stepcompare::import::AssemblyNode node;
    node.id = "root";
    node.prototypeId = "part";
    node.nameUtf8 = "Part";
    node.localTransform.matrix[3] = translationX;
    model.nodes.push_back(std::move(node));
    return model;
}

ImportedModel assemblyModel(std::u8string path, double secondPositionX) {
    ImportedModel model;
    model.sourcePathUtf8 = std::move(path);
    model.lengthUnit.status =
        stepcompare::import::UnitNormalizationStatus::NormalizedToMillimetres;
    model.rootNodeIds = {"assembly"};
    double width = 10.0;
    for (const std::string id : {"prototype-1", "prototype-2"}) {
        stepcompare::import::PartPrototype prototype;
        prototype.id = id;
        prototype.nameUtf8 = id;
        prototype.statistics = boxStatistics(width);
        prototype.geometry = std::make_shared<FakeGeometry>();
        model.prototypes.push_back(std::move(prototype));
        width += 2.0;
    }
    stepcompare::import::AssemblyNode root;
    root.id = "assembly";
    root.childIds = {"component-1", "component-2"};
    root.nameUtf8 = "Assembly";
    root.isAssembly = true;
    model.nodes.push_back(std::move(root));

    stepcompare::import::AssemblyNode first;
    first.id = "component-1";
    first.parentId = "assembly";
    first.prototypeId = "prototype-1";
    first.nameUtf8 = "Part 1";
    first.isInstance = true;
    model.nodes.push_back(std::move(first));

    stepcompare::import::AssemblyNode second;
    second.id = "component-2";
    second.parentId = "assembly";
    second.prototypeId = "prototype-2";
    second.nameUtf8 = "Part 2";
    second.localTransform.matrix[3] = secondPositionX;
    second.isInstance = true;
    model.nodes.push_back(std::move(second));
    return model;
}

ImportedModel repeatedAssemblyModel(std::u8string path) {
    auto model = assemblyModel(std::move(path), 25.0);
    model.prototypes.erase(model.prototypes.begin() + 1);
    model.nodes[2].prototypeId = "prototype-1";
    return model;
}

class MockImporter final : public stepcompare::import::StepImportPort {
public:
    std::deque<StepImportResult> results;
    int calls{};

    StepImportResult importStep(const StepImportRequest&) noexcept override {
        ++calls;
        if (results.empty()) {
            return {};
        }
        auto result = std::move(results.front());
        results.pop_front();
        return result;
    }
};

class MockDeepGeometry final : public stepcompare::deep::DeepGeometryPort {
public:
    DeepGeometryResult result;
    int calls{};

    DeepGeometryResult compareAligned(
        const DeepGeometryRequest&) noexcept override {
        ++calls;
        return result;
    }
};

class MockSurfaceDeviation final
    : public stepcompare::deviation::SurfaceDeviationPort {
public:
    stepcompare::deviation::SurfaceDeviationResult result{
        .status = stepcompare::deviation::SurfaceDeviationStatus::WithinTolerance,
        .maximumMm = 0.001,
        .meanMm = 0.0005,
        .rmsMm = 0.0006,
        .percentileMm = 0.0009,
        .samplesAToB = 8,
        .samplesBToA = 8,
        .trianglesA = 12,
        .trianglesB = 12,
        .triangleDistanceEvaluations = 64,
    };
    int calls{};

    stepcompare::deviation::SurfaceDeviationResult compare(
        const stepcompare::deviation::SurfaceDeviationRequest&) noexcept override {
        ++calls;
        return result;
    }
};

class MockFeatureRecognizer final
    : public stepcompare::feature::FeatureRecognitionPort {
public:
    int calls{};
    double secondCenterShiftX{};
    double secondPrimarySizeDelta{};
    double secondDepthDelta{};
    double secondRadiusDelta{};
    double secondAngleDelta{};
    bool firstAmbiguous{};
    bool secondAmbiguous{};
    bool firstEmpty{};
    bool secondEmpty{};
    std::stop_source* stopSourceToRequest{};

    stepcompare::feature::FeatureRecognitionResult recognize(
        const stepcompare::import::GeometryPayloadPtr&,
        double,
        double,
        std::stop_token cancellation) noexcept override {
        if (cancellation.stop_requested()) {
            return {.cancelled = true};
        }
        ++calls;
        if (calls == 1 && stopSourceToRequest != nullptr) {
            stopSourceToRequest->request_stop();
            return {.cancelled = true};
        }
        if ((calls == 1 && firstEmpty) || (calls == 2 && secondEmpty)) {
            return {.completed = true};
        }
        stepcompare::feature::RecognizedFeature hole;
        hole.stableId = "feature/face/7";
        hole.type = stepcompare::feature::FeatureType::ThroughHole;
        hole.evidence = (calls == 1 ? firstAmbiguous : secondAmbiguous)
                            ? stepcompare::feature::RecognitionEvidence::Ambiguous
                            : stepcompare::feature::RecognitionEvidence::GeometryProven;
        hole.confidence = 0.98;
        hole.centerLocalMm = {
            2.0 + (calls == 2 ? secondCenterShiftX : 0.0), 3.0, 4.0};
        hole.axis = {0.0, 0.0, 1.0};
        hole.primarySizeMm = 8.0 + (calls == 2 ? secondPrimarySizeDelta : 0.0);
        hole.depthMm = 20.0 + (calls == 2 ? secondDepthDelta : 0.0);
        hole.radiusMm = 4.0 + (calls == 2 ? secondRadiusDelta : 0.0);
        hole.angleDegrees = calls == 2 ? secondAngleDelta : 0.0;
        hole.profile = "CIRCULAR";
        hole.through = true;
        hole.faceIndices = {7};
        return {.completed = true, .features = {std::move(hole)}};
    }
};

StepImportResult success(ImportedModel model) {
    StepImportResult result;
    result.model = std::move(model);
    return result;
}

void exactPartDeepPassTest() {
    MockImporter importer;
    importer.results.push_back(success(partModel(u8"A.step")));
    importer.results.push_back(success(partModel(u8"B.step")));
    MockDeepGeometry deep;
    deep.result.status = DeepGeometryStatus::SameGeometry;
    deep.result.alignmentProven = true;
    deep.result.volumeAMm3 = 6000.0;
    deep.result.volumeBMm3 = 6000.0;
    deep.result.commonVolumeMm3 = 6000.0;

    MockSurfaceDeviation surface;
    ComparisonCoordinator coordinator(importer, deep, &surface);
    const auto result = coordinator.compare(
        ComparisonRequest{u8"A.step", u8"B.step", {}, true});
    expect(result.status == ComparisonRunStatus::Completed,
           "deep exact part run must complete");
    expect(result.verdict.decision == Decision::Pass,
           "deep-proven exact part must PASS");
    expect(deep.calls == 1, "prototype pair must be deep checked once");
    expect(surface.calls == 1 && result.report.deepDeviation.available,
           "canonical PASS must include quantitative surface deviation");
    expect(std::abs(result.report.deepDeviation.maximumMm - 0.001) < 1.0e-12 &&
               std::abs(result.report.deepDeviation.meanMm - 0.0005) < 1.0e-12 &&
               std::abs(result.report.deepDeviation.rmsMm - 0.0006) < 1.0e-12,
           "canonical report must carry max/mean/RMS deviation");
    expect(result.report.verdict.decision == "PASS",
           "canonical report must contain PASS");
    expect(result.report.components.size() == 1,
           "part comparison must emit one component row");
    expect(stepcompare::reporting::toJson(result.report).find(
               "\"decision\":\"PASS\"") != std::string::npos,
           "JSON must serialize the canonical verdict");
    expect(stepcompare::reporting::toCsv(result.report).find(
               "verdict,decision,PASS") != std::string::npos,
           "CSV must serialize the canonical verdict");
}

void movedPartFailsTest() {
    MockImporter importer;
    importer.results.push_back(success(partModel(u8"A.step")));
    importer.results.push_back(success(partModel(u8"B.step", 10.0, 5.0)));
    MockDeepGeometry deep;
    deep.result.status = DeepGeometryStatus::SameGeometry;
    deep.result.alignmentProven = true;

    MockSurfaceDeviation surface;
    ComparisonCoordinator coordinator(importer, deep, &surface);
    const auto result = coordinator.compare(
        ComparisonRequest{u8"A.step", u8"B.step", {}, true});
    expect(result.verdict.decision == Decision::Fail,
           "same geometry moved component must FAIL");
    expect(result.report.verdict.reasons.size() == 1 &&
               result.report.verdict.reasons.front() ==
                   "SAME_GEOMETRY_POSITION_CHANGED",
           "moved component must use canonical position reason");
    expect(result.report.components.front().positionStatus == "MOVED",
           "component report must retain MOVED status");
    expect(std::abs(result.report.components.front()
                        .translationBMinusAMm.x - 5.0) < 1.0e-12,
           "component report delta must follow B-A convention");
}

void movedPartKeepsAlignedFeaturePlacementPass() {
    MockImporter importer;
    importer.results.push_back(success(partModel(u8"A.step")));
    importer.results.push_back(success(partModel(u8"B.step", 10.0, 5.0)));
    MockDeepGeometry deep;
    deep.result.status = DeepGeometryStatus::SameGeometry;
    deep.result.alignmentProven = true;
    MockSurfaceDeviation surface;
    MockFeatureRecognizer features;
    ComparisonCoordinator coordinator(importer, deep, &surface, &features);
    const auto result = coordinator.compare(
        ComparisonRequest{u8"A.step", u8"B.step", {}, true});

    expect(result.verdict.decision == Decision::Fail,
           "absolute component move must preserve canonical whole-model FAIL");
    expect(result.report.features.size() == 1,
           "deep comparison must append one feature evidence row");
    if (!result.report.features.empty()) {
        const auto& feature = result.report.features.front();
        expect(std::abs(feature.absoluteDifferenceBMinusAMm.x - 5.0) < 1.0e-12,
               "absolute feature difference must retain the owner move B-A");
        expect(std::abs(feature.alignedDifferenceBMinusAMm.x) < 1.0e-12 &&
                   feature.result == "PASS" &&
                   feature.evidenceStatus == "GEOMETRY_PROVEN",
               "rigid owner move must align away without false feature placement FAIL");
        expect(stepcompare::reporting::toJson(result.report).find(
                   "\"alignedDifferenceBMinusAMm\"") != std::string::npos,
               "canonical JSON must expose aligned feature evidence additively");
    }
}

void alignedFeaturePlacementChangeFailsFeatureEvidence() {
    MockImporter importer;
    importer.results.push_back(success(partModel(u8"A.step")));
    importer.results.push_back(success(partModel(u8"B.step")));
    MockDeepGeometry deep;
    deep.result.status = DeepGeometryStatus::SameGeometry;
    deep.result.alignmentProven = true;
    MockSurfaceDeviation surface;
    MockFeatureRecognizer features;
    features.secondCenterShiftX = 1.0;
    ComparisonCoordinator coordinator(importer, deep, &surface, &features);
    const auto result = coordinator.compare(
        ComparisonRequest{u8"A.step", u8"B.step", {}, true});
    expect(result.report.features.size() == 1 &&
                   result.report.features.front().result == "FAIL" &&
                   result.report.features.front().reason ==
                       "FEATURE_POSITION_CHANGED" &&
               std::abs(result.report.features.front()
                            .alignedDifferenceBMinusAMm.x - 1.0) < 1.0e-12,
           "feature placement remaining after rigid alignment must use the explicit position reason");
}

void featureReasonTaxonomyTests() {
    const auto compareWith = [](auto configure) {
        MockImporter importer;
        importer.results.push_back(success(partModel(u8"A.step")));
        importer.results.push_back(success(partModel(u8"B.step")));
        MockDeepGeometry deep;
        deep.result.status = DeepGeometryStatus::SameGeometry;
        deep.result.alignmentProven = true;
        MockSurfaceDeviation surface;
        MockFeatureRecognizer features;
        configure(features);
        ComparisonCoordinator coordinator(importer, deep, &surface, &features);
        return coordinator.compare(
            ComparisonRequest{u8"A.step", u8"B.step", {}, true});
    };

    const auto size = compareWith(
        [](auto& features) { features.secondPrimarySizeDelta = 1.0; });
    expect(size.report.features.front().reason == "FEATURE_SIZE_CHANGED",
           "feature dimension changes must use FEATURE_SIZE_CHANGED");

    const auto depth = compareWith(
        [](auto& features) { features.secondDepthDelta = 1.0; });
    expect(depth.report.features.front().reason == "FEATURE_DEPTH_CHANGED",
           "feature depth changes must use FEATURE_DEPTH_CHANGED");

    const auto radius = compareWith(
        [](auto& features) { features.secondRadiusDelta = 1.0; });
    expect(radius.report.features.front().reason == "FEATURE_RADIUS_CHANGED",
           "feature radius changes must use FEATURE_RADIUS_CHANGED");

    const auto angle = compareWith(
        [](auto& features) { features.secondAngleDelta = 1.0; });
    expect(angle.report.features.front().reason == "FEATURE_ANGLE_CHANGED",
           "feature angle changes must use FEATURE_ANGLE_CHANGED");
}

void ambiguousUnmatchedRemainsCheckTests() {
    const auto run = [](const bool ambiguousA) {
        MockImporter importer;
        importer.results.push_back(success(partModel(u8"A.step")));
        importer.results.push_back(success(partModel(u8"B.step")));
        MockDeepGeometry deep;
        deep.result.status = DeepGeometryStatus::SameGeometry;
        deep.result.alignmentProven = true;
        MockSurfaceDeviation surface;
        MockFeatureRecognizer features;
        features.firstAmbiguous = ambiguousA;
        features.secondAmbiguous = !ambiguousA;
        features.firstEmpty = !ambiguousA;
        features.secondEmpty = ambiguousA;
        ComparisonCoordinator coordinator(importer, deep, &surface, &features);
        return coordinator.compare(
            ComparisonRequest{u8"A.step", u8"B.step", {}, true});
    };

    for (const bool ambiguousA : {true, false}) {
        const auto result = run(ambiguousA);
        expect(result.report.features.size() == 1 &&
                   result.report.features.front().result == "CHECK" &&
                   result.report.features.front().reason == "FEATURE_AMBIGUOUS",
               "an unmatched ambiguous feature on either side must remain CHECK");
    }
}

void featureStageCancellationTest() {
    MockImporter importer;
    importer.results.push_back(success(partModel(u8"A.step")));
    importer.results.push_back(success(partModel(u8"B.step")));
    MockDeepGeometry deep;
    deep.result.status = DeepGeometryStatus::SameGeometry;
    deep.result.alignmentProven = true;
    MockSurfaceDeviation surface;
    MockFeatureRecognizer features;
    std::stop_source cancellation;
    features.stopSourceToRequest = &cancellation;
    ComparisonCoordinator coordinator(importer, deep, &surface, &features);
    ComparisonRequest request{u8"A.step", u8"B.step", {}, true};
    request.cancellation = cancellation.get_token();
    const auto result = coordinator.compare(request);
    expect(result.status == ComparisonRunStatus::Cancelled &&
               result.report.execution.cancellationRequested &&
               result.report.execution.status == "CANCELLED",
           "cancellation received inside feature recognition must prevent COMPLETED publication");
}

void fastOnlyIsCheckTest() {
    MockImporter importer;
    importer.results.push_back(success(partModel(u8"A.step")));
    importer.results.push_back(success(partModel(u8"B.step")));
    MockDeepGeometry deep;
    MockSurfaceDeviation surface;
    ComparisonCoordinator coordinator(importer, deep, &surface);
    const auto result = coordinator.compare(
        ComparisonRequest{u8"A.step", u8"B.step", {}, false});
    expect(result.verdict.decision == Decision::Check,
           "compatible fast invariants alone must not PASS");
    expect(deep.calls == 0, "fast-only run must not invoke deep port");
    expect(result.report.verdict.reasons.front() == "EVIDENCE_INCOMPLETE",
           "fast-only report must expose incomplete evidence");
}

void assemblyMatchingTest() {
    MockImporter importer;
    importer.results.push_back(success(assemblyModel(u8"A.step", 20.0)));
    importer.results.push_back(success(assemblyModel(u8"B.step", 25.0)));
    MockDeepGeometry deep;
    deep.result.status = DeepGeometryStatus::SameGeometry;
    deep.result.alignmentProven = true;
    MockSurfaceDeviation surface;
    ComparisonCoordinator coordinator(importer, deep, &surface);
    const auto result = coordinator.compare(
        ComparisonRequest{u8"A.step", u8"B.step", {}, true});
    if (result.verdict.decision != Decision::Fail) {
        std::cerr << "ASSEMBLY VERDICT: " << result.report.verdict.decision;
        for (const auto& reason : result.report.verdict.reasons) {
            std::cerr << ' ' << reason;
        }
        std::cerr << '\n';
        for (const auto& component : result.report.components) {
            std::cerr << "COMPONENT " << component.idA << " -> "
                      << component.idB << ' ' << component.matchStatus << ' '
                      << component.geometryStatus << ' '
                      << component.positionStatus << '\n';
        }
    }
    expect(result.verdict.decision == Decision::Fail,
           "assembly with one moved component must FAIL");
    expect(result.report.components.size() == 2,
           "assembly matcher must emit both component rows");
    const auto moved = std::find_if(
        result.report.components.begin(),
        result.report.components.end(),
        [](const auto& component) {
            return component.positionStatus == "MOVED";
        });
    expect(moved != result.report.components.end(),
           "assembly matcher must identify the moved component");
    if (moved != result.report.components.end()) {
        expect(std::abs(moved->translationBMinusAMm.x - 5.0) < 1.0e-12,
               "assembly component delta must follow B-A convention");
    }
}

void changedGeometryFailsTest() {
    MockImporter importer;
    importer.results.push_back(success(partModel(u8"A.step", 10.0)));
    importer.results.push_back(success(partModel(u8"B.step", 11.0)));
    MockDeepGeometry deep;
    MockSurfaceDeviation surface;
    ComparisonCoordinator coordinator(importer, deep, &surface);
    const auto result = coordinator.compare(
        ComparisonRequest{u8"A.step", u8"B.step", {}, true});
    expect(result.verdict.decision == Decision::Fail,
           "fast-proven geometry difference must FAIL");
    expect(deep.calls == 0,
           "fast-proven different prototype pair must not require deep work");
}

void ambiguityAndFailureTests() {
    {
        MockImporter importer;
        importer.results.push_back(success(partModel(u8"A.step")));
        importer.results.push_back(success(partModel(u8"B.step")));
        MockDeepGeometry deep;
        deep.result.status = DeepGeometryStatus::AlignmentNotProven;
        MockSurfaceDeviation surface;
        ComparisonCoordinator coordinator(importer, deep, &surface);
        const auto result = coordinator.compare(
            ComparisonRequest{u8"A.step", u8"B.step", {}, true});
        expect(result.verdict.decision == Decision::Check,
               "unproven alignment must CHECK");
        expect(result.report.verdict.reasons.front() == "ALIGNMENT_AMBIGUOUS",
               "unproven alignment must be explicit in report");
    }
    {
        MockImporter importer;
        StepImportResult failed;
        failed.diagnostics.push_back({
            stepcompare::import::ImportDiagnosticSeverity::Error,
            stepcompare::import::ImportDiagnosticCode::StepReadFailed,
            "bad STEP"});
        importer.results.push_back(std::move(failed));
        importer.results.push_back(success(partModel(u8"B.step")));
        MockDeepGeometry deep;
        MockSurfaceDeviation surface;
        ComparisonCoordinator coordinator(importer, deep, &surface);
        const auto result = coordinator.compare(
            ComparisonRequest{u8"A.step", u8"B.step", {}, true});
        expect(result.status == ComparisonRunStatus::InputError,
               "import failure must be classified as input error");
        expect(result.verdict.decision == Decision::Error,
               "import failure must produce ERROR verdict");
        expect(result.report.verdict.reasons.front() == "STEP_IMPORT_FAILED",
               "import failure report reason must be canonical");
    }
}

void cancellationTest() {
    MockImporter importer;
    importer.results.push_back(success(partModel(u8"A.step")));
    importer.results.push_back(success(partModel(u8"B.step")));
    MockDeepGeometry deep;
    std::stop_source cancellation;
    cancellation.request_stop();
    ComparisonRequest request{u8"A.step", u8"B.step", {}, true};
    request.cancellation = cancellation.get_token();
    MockSurfaceDeviation surface;
    ComparisonCoordinator coordinator(importer, deep, &surface);
    const auto result = coordinator.compare(request);
    expect(result.status == ComparisonRunStatus::Cancelled,
           "pre-cancelled comparison must stop at a checkpoint");
    expect(importer.calls == 0,
           "pre-cancelled comparison must not start import");
    expect(result.report.verdict.reasons.front() == "CANCELLED",
           "cancelled result must be explicit in report");
}

void cacheIntegrationTest() {
    MockImporter importer;
    importer.results.push_back(success(partModel(u8"A.step")));
    importer.results.push_back(success(partModel(u8"B.step")));
    MockDeepGeometry deep;
    deep.result.status = DeepGeometryStatus::SameGeometry;
    deep.result.alignmentProven = true;
    MockSurfaceDeviation surface;
    ComparisonCoordinator coordinator(importer, deep, &surface, 1024U * 1024U);

    ComparisonRequest request{u8"A.step", u8"B.step", {}, true};
    request.identityA = stepcompare::cache::FileIdentity{
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        100U,
        1};
    request.identityB = stepcompare::cache::FileIdentity{
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        200U,
        2};
    const auto first = coordinator.compare(request);
    const auto second = coordinator.compare(request);
    expect(first.status == ComparisonRunStatus::Completed &&
               !first.report.cache.hit && first.report.cache.enabled,
           "first canonical comparison must record a cache miss");
    expect(second.status == ComparisonRunStatus::Completed &&
               second.report.cache.hit,
           "second identical comparison must be served from cache");
    expect(importer.calls == 2 && deep.calls == 1 && surface.calls == 1,
           "cache hit must skip import, deep geometry, and deviation work");
    expect(second.report.cache.hits >= 1 && second.report.cache.misses >= 1,
           "canonical report must expose cache hit/miss statistics");
}

void surfaceCancellationAndFailureTests() {
    {
        MockImporter importer;
        importer.results.push_back(success(partModel(u8"A.step")));
        importer.results.push_back(success(partModel(u8"B.step")));
        MockDeepGeometry deep;
        deep.result.status = DeepGeometryStatus::SameGeometry;
        deep.result.alignmentProven = true;
        MockSurfaceDeviation surface;
        surface.result.status =
            stepcompare::deviation::SurfaceDeviationStatus::Cancelled;
        ComparisonCoordinator coordinator(importer, deep, &surface);
        const auto result = coordinator.compare(
            ComparisonRequest{u8"A.step", u8"B.step", {}, true});
        expect(result.status == ComparisonRunStatus::Cancelled &&
                   result.report.execution.cancellationRequested,
               "cancelled surface stage must never publish a completed result");
    }
    {
        MockImporter importer;
        importer.results.push_back(success(partModel(u8"A.step")));
        importer.results.push_back(success(partModel(u8"B.step")));
        MockDeepGeometry deep;
        deep.result.status = DeepGeometryStatus::SameGeometry;
        deep.result.alignmentProven = true;
        MockSurfaceDeviation surface;
        surface.result.status =
            stepcompare::deviation::SurfaceDeviationStatus::Error;
        ComparisonCoordinator coordinator(importer, deep, &surface);
        const auto result = coordinator.compare(
            ComparisonRequest{u8"A.step", u8"B.step", {}, true});
        expect(result.verdict.decision != Decision::Pass &&
                   result.status == ComparisonRunStatus::ProcessingError,
               "missing surface evidence must fail closed and forbid PASS");
    }
}

void byteIdenticalAssemblyProofTest() {
    MockImporter importer;
    importer.results.push_back(success(repeatedAssemblyModel(u8"A.step")));
    importer.results.push_back(success(repeatedAssemblyModel(u8"B.step")));
    MockDeepGeometry deep;
    MockSurfaceDeviation surface;
    ComparisonCoordinator coordinator(importer, deep, &surface);
    ComparisonRequest request{u8"A.step", u8"B.step", {}, true};
    request.identityA = stepcompare::cache::FileIdentity{
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        300U,
        1};
    request.identityB = stepcompare::cache::FileIdentity{
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        300U,
        2};
    const auto result = coordinator.compare(request);
    expect(result.status == ComparisonRunStatus::Completed &&
               result.verdict.decision == Decision::Pass,
           "byte-identical assemblies must use exact identity proof");
    expect(result.report.components.size() == 2U &&
               result.report.deepDeviation.available &&
               result.report.deepDeviation.maximumMm == 0.0 &&
               result.report.deepDeviation.sampleCount == 0U,
           "identity proof must publish truthful zero deviation without fake samples");
    expect(deep.calls == 0 && surface.calls == 0,
           "identity proof must not pretend to run geometry sampling");
}

}  // namespace

int main() {
    exactPartDeepPassTest();
    movedPartFailsTest();
    movedPartKeepsAlignedFeaturePlacementPass();
    alignedFeaturePlacementChangeFailsFeatureEvidence();
    featureReasonTaxonomyTests();
    ambiguousUnmatchedRemainsCheckTests();
    featureStageCancellationTest();
    fastOnlyIsCheckTest();
    assemblyMatchingTest();
    changedGeometryFailsTest();
    ambiguityAndFailureTests();
    cancellationTest();
    cacheIntegrationTest();
    surfaceCancellationAndFailureTests();
    byteIdenticalAssemblyProofTest();
    if (failures != 0) {
        std::cerr << failures << " application assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All application coordinator tests passed\n";
    return EXIT_SUCCESS;
}
