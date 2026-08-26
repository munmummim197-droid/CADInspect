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

    ComparisonCoordinator coordinator(importer, deep);
    const auto result = coordinator.compare(
        ComparisonRequest{u8"A.step", u8"B.step", {}, true});
    expect(result.status == ComparisonRunStatus::Completed,
           "deep exact part run must complete");
    expect(result.verdict.decision == Decision::Pass,
           "deep-proven exact part must PASS");
    expect(deep.calls == 1, "prototype pair must be deep checked once");
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

    ComparisonCoordinator coordinator(importer, deep);
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

void fastOnlyIsCheckTest() {
    MockImporter importer;
    importer.results.push_back(success(partModel(u8"A.step")));
    importer.results.push_back(success(partModel(u8"B.step")));
    MockDeepGeometry deep;
    ComparisonCoordinator coordinator(importer, deep);
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
    ComparisonCoordinator coordinator(importer, deep);
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
    ComparisonCoordinator coordinator(importer, deep);
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
        ComparisonCoordinator coordinator(importer, deep);
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
        ComparisonCoordinator coordinator(importer, deep);
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
    ComparisonCoordinator coordinator(importer, deep);
    const auto result = coordinator.compare(request);
    expect(result.status == ComparisonRunStatus::Cancelled,
           "pre-cancelled comparison must stop at a checkpoint");
    expect(importer.calls == 0,
           "pre-cancelled comparison must not start import");
    expect(result.report.verdict.reasons.front() == "CANCELLED",
           "cancelled result must be explicit in report");
}

}  // namespace

int main() {
    exactPartDeepPassTest();
    movedPartFailsTest();
    fastOnlyIsCheckTest();
    assemblyMatchingTest();
    changedGeometryFailsTest();
    ambiguityAndFailureTests();
    cancellationTest();
    if (failures != 0) {
        std::cerr << failures << " application assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All application coordinator tests passed\n";
    return EXIT_SUCCESS;
}
