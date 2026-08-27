#include "comparison_readability_model.hpp"
#include "dropped_step_files.hpp"
#include "pair_isolation_model.hpp"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

stepcompare::reporting::Report sampleReport() {
    using stepcompare::reporting::ComponentRow;
    stepcompare::reporting::Report report;
    report.verdict.decision = "PASS";
    report.verdict.reasons = {"SAME_GEOMETRY_SAME_POSITION"};
    report.tolerances.positionMm = 0.01;
    report.tolerances.surfaceMm = 0.01;
    report.tolerances.angularDegrees = 0.01;
    report.statisticsA.sizeMm = {1000.0, 20.0, 30.0};
    report.statisticsB.sizeMm = {1001.25, 20.0, 30.0};
    report.statisticsA.centerOfMassMm = {10.0, 20.0, 30.0};
    report.statisticsB.centerOfMassMm = {15.0, 20.0, 30.0};
    report.statisticsA.volumeMm3 = 1234567.0;
    report.statisticsB.volumeMm3 = 1234567.0;
    report.statisticsA.surfaceAreaMm2 = 9000.0;
    report.statisticsB.surfaceAreaMm2 = 9000.0;
    report.statisticsA.solidCount = report.statisticsB.solidCount = 1;
    report.statisticsA.shellCount = report.statisticsB.shellCount = 1;
    report.statisticsA.faceCount = report.statisticsB.faceCount = 6;
    report.statisticsA.edgeCount = report.statisticsB.edgeCount = 12;
    report.statisticsA.vertexCount = report.statisticsB.vertexCount = 8;
    report.placement.translationBMinusAMm = {5.0, 0.0, 0.0};
    report.placement.rotationAngleDegrees = 0.5;
    report.placement.displayEulerDegrees = {0.0, 0.0, 0.5};
    report.deepDeviation = {.available = true,
                            .maximumMm = 0.02,
                            .meanMm = 0.005,
                            .rmsMm = 0.008,
                            .percentile95Mm = 0.015};
    report.inputA.pathUtf8 = "D:/A.step";
    report.inputB.pathUtf8 = "D:/B.step";
    report.inputA.sha256 = std::string(64, 'a');
    report.inputB.sha256 = std::string(64, 'b');
    report.inputA.sizeBytes = 1200000;
    report.inputB.sizeBytes = 1200100;

    const auto component = [](std::string id,
                              std::string status,
                              std::string geometry = "SAME_PROVEN",
                              std::string match = "MATCH_EXACT") {
        ComponentRow row;
        row.idA = "a-" + id;
        row.idB = "b-" + id;
        row.nameA = row.nameB = id;
        row.positionStatus = std::move(status);
        row.geometryStatus = std::move(geometry);
        row.matchStatus = std::move(match);
        row.deviation.available = true;
        row.deviation.maximumMm = 0.005;
        return row;
    };
    report.components = {
        component("same", "SAME"),
        component("moved", "MOVED"),
        component("rotated", "ROTATED"),
        component("both", "MOVED_AND_ROTATED"),
        component("geometry", "UNKNOWN", "DIFFERENT_PROVEN"),
        component("ambiguous", "UNKNOWN", "INCONCLUSIVE", "AMBIGUOUS"),
    };
    auto missing = component("missing", "UNKNOWN");
    missing.idB.clear();
    report.components.push_back(std::move(missing));
    auto added = component("new", "UNKNOWN");
    added.idA.clear();
    report.components.push_back(std::move(added));

    stepcompare::reporting::FeatureRow feature;
    feature.idA = "feature/face/7";
    feature.idB = "feature/face/7";
    feature.ownerComponentIdA = "a-same";
    feature.ownerComponentIdB = "b-same";
    feature.type = "THROUGH_HOLE";
    feature.evidenceStatus = "AMBIGUOUS";
    feature.result = "CHECK";
    feature.reason = "FEATURE_AMBIGUOUS";
    feature.centerAAbsoluteMm = {10.0, 20.0, 30.0};
    feature.centerBAbsoluteMm = {15.0, 20.0, 30.0};
    feature.centerBAlignedMm = {10.0, 20.0, 30.0};
    feature.absoluteDifferenceBMinusAMm = {5.0, 0.0, 0.0};
    feature.alignedDifferenceBMinusAMm = {0.0, 0.0, 0.0};
    feature.axisA = feature.axisB = feature.axisBAligned = {0.0, 0.0, 1.0};
    feature.primarySizeAMm = feature.primarySizeBMm = 10.0;
    feature.depthAMm = feature.depthBMm = 30.0;
    feature.radiusAMm = feature.radiusBMm = 5.0;
    feature.profileA = feature.profileB = "CIRCULAR";
    feature.throughA = feature.throughB = true;
    feature.positionToleranceMm = 0.01;
    feature.angularToleranceDegrees = 0.01;
    feature.faceIndicesA = feature.faceIndicesB = {7};
    report.features.push_back(std::move(feature));
    return report;
}

void vietnameseOverallVerdictIsExplicit() {
    auto report = sampleReport();
    auto value = stepcompare::gui::presentOverallVerdict(report);
    expect(value.title.contains(QStringLiteral("GIỐNG NHAU")),
           "PASS must be presented as GIỐNG NHAU");

    report.verdict.decision = "FAIL";
    report.verdict.reasons = {"SAME_GEOMETRY_POSITION_CHANGED"};
    value = stepcompare::gui::presentOverallVerdict(report);
    expect(value.title.contains(QStringLiteral("CÙNG HÌNH HỌC / KHÁC VỊ TRÍ")),
           "position-only FAIL must be explicit in Vietnamese");

    report.verdict.reasons = {"GEOMETRY_CHANGED"};
    value = stepcompare::gui::presentOverallVerdict(report);
    expect(value.title.contains(QStringLiteral("HÌNH HỌC THAY ĐỔI")),
           "geometry FAIL must be explicit in Vietnamese");

    report.verdict.decision = "CHECK";
    value = stepcompare::gui::presentOverallVerdict(report);
    expect(value.title.contains(QStringLiteral("CHECK / MƠ HỒ")),
           "CHECK must remain visibly fail-closed");

    report.verdict.decision = "ERROR";
    value = stepcompare::gui::presentOverallVerdict(report);
    expect(value.title.contains(QStringLiteral("ERROR")),
           "ERROR must remain explicit");
}

void parametersHaveRequiredColumnsGroupsAndFormatting() {
    auto report = sampleReport();
    stepcompare::gui::ComparisonParameterModel model;
    model.setReport(report);
    expect(model.columnCount() == 6, "parameter table must have six columns");
    expect(model.headerData(0, Qt::Horizontal).toString() == QStringLiteral("Thông số"),
           "first parameter header must be Vietnamese");
    expect(model.headerData(3, Qt::Horizontal).toString() ==
               QStringLiteral("Sai lệch B - A"),
           "difference convention must be visible in the header");

    int groups = 0;
    bool hasDeltaX = false;
    bool hasMaximum = false;
    bool fixedNoScientific = false;
    for (int row = 0; row < model.rowCount(); ++row) {
        groups += model.isGroupHeader(row) ? 1 : 0;
        const QString label = model.data(model.index(row, 0)).toString();
        hasDeltaX = hasDeltaX || label == QStringLiteral("ΔX (B - A)");
        hasMaximum = hasMaximum || label == QStringLiteral("Max deviation");
        if (label == QStringLiteral("Volume")) {
            const QString value = model.data(model.index(row, 1)).toString();
            fixedNoScientific = !value.contains(QLatin1Char('e'), Qt::CaseInsensitive) &&
                                value.contains(QStringLiteral("mm³"));
        }
    }
    expect(groups == 6, "parameter rows must be divided into six required groups");
    expect(hasDeltaX, "parameter table must show explicit Delta X (B - A)");
    expect(hasMaximum, "parameter table must show maximum surface deviation");
    expect(fixedNoScientific, "large values must use fixed readable formatting");
    expect(model.data(model.index(1, 1), Qt::TextAlignmentRole).toInt() & Qt::AlignRight,
           "numeric parameter cells must be right aligned");
    bool defaultTwoDecimals = false;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.data(model.index(row, 0)).toString() ==
            QStringLiteral("ΔX (B - A)")) {
            const QString value = model.data(model.index(row, 3)).toString();
            defaultTwoDecimals = value.contains(QStringLiteral("5,00 mm")) &&
                                 !value.contains(QStringLiteral("5,0000"));
        }
    }
    expect(defaultTwoDecimals,
           "millimetre display precision must default to two decimals");
}

void componentModelFiltersAndStableIdsScale() {
    auto report = sampleReport();
    stepcompare::gui::ComponentComparisonModel model;
    stepcompare::gui::ComponentFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    model.setReport(report);
    expect(model.columnCount() == 7, "component table must have seven columns");
    expect(model.rowCount() == 8, "all canonical component rows must be published");

    proxy.setComponentFilter(stepcompare::gui::ComponentFilter::DifferencesOnly);
    expect(proxy.rowCount() == 7, "differences filter must exclude unchanged rows");
    proxy.setComponentFilter(stepcompare::gui::ComponentFilter::Moved);
    expect(proxy.rowCount() == 2, "Moved must include moved-and-rotated rows");
    proxy.setComponentFilter(stepcompare::gui::ComponentFilter::Rotated);
    expect(proxy.rowCount() == 2, "Rotated must include moved-and-rotated rows");
    proxy.setComponentFilter(stepcompare::gui::ComponentFilter::GeometryChanged);
    expect(proxy.rowCount() == 1, "Geometry Changed filter must be exact");
    proxy.setComponentFilter(stepcompare::gui::ComponentFilter::Missing);
    expect(proxy.rowCount() == 1, "Missing filter must be exact");
    proxy.setComponentFilter(stepcompare::gui::ComponentFilter::Added);
    expect(proxy.rowCount() == 1, "New filter must be exact");
    proxy.setComponentFilter(stepcompare::gui::ComponentFilter::Ambiguous);
    expect(proxy.rowCount() == 1, "Ambiguous filter must be exact");

    const QModelIndex moved = model.indexForStableId("preview/B/b-moved");
    expect(moved.isValid(), "stable IDs from either side must locate the same row");
    expect(model.preferredStableId(moved) == "preview/A/a-moved",
           "paired component selection must use a deterministic viewer ID");

    report.components.clear();
    report.components.reserve(5000);
    for (int index = 0; index < 5000; ++index) {
        stepcompare::reporting::ComponentRow row;
        row.idA = row.idB = std::to_string(index);
        row.nameA = row.nameB = "Occurrence " + std::to_string(index);
        row.matchStatus = "MATCH_EXACT";
        row.geometryStatus = "SAME_PROVEN";
        row.positionStatus = "SAME";
        report.components.push_back(std::move(row));
    }
    model.setReport(report);
    expect(model.rowCount() == 5000,
           "model/view layer must retain 5000 rows without per-row widgets");

    std::vector<stepcompare::gui::PreviewPartIdentity> identities;
    identities.reserve(10'000);
    for (int index = 0; index < 5000; ++index) {
        const std::string id = std::to_string(index);
        identities.push_back({.stableId = "preview/A/" + id,
                              .prototypeId = "prototype-A",
                              .partName = QStringLiteral("Benchmark Part"),
                              .side = stepcompare::viewer::ModelSide::A});
        identities.push_back({.stableId = "preview/B/" + id,
                              .prototypeId = "prototype-B",
                              .partName = QStringLiteral("Benchmark Part"),
                              .side = stepcompare::viewer::ModelSide::B});
    }
    stepcompare::gui::PartComparisonModel partModel;
    partModel.setReport(report, identities);
    expect(partModel.rowCount() == 1,
           "5000 repeated occurrences must collapse into one Part summary row");
    expect(partModel.data(partModel.index(
               0, stepcompare::gui::PartComparisonModel::QuantityA)).toString() ==
               QStringLiteral("5.000") &&
               partModel.data(partModel.index(
                   0, stepcompare::gui::PartComparisonModel::QuantityB)).toString() ==
                   QStringLiteral("5.000"),
           "Part summary must expose localized A/B occurrence quantities");
    expect(partModel.occurrences(partModel.index(0, 0)).size() == 5000,
           "Part master-detail must retain all occurrence rows in a model");
    expect(partModel.indexForStableId("preview/B/4999").isValid(),
           "tree/viewer stable IDs must locate their Part summary");
}

void featureModelIsFailClosedAndShowsBothCoordinateFrames() {
    const auto report = sampleReport();
    stepcompare::gui::FeatureComparisonModel model;
    model.setReport(report);
    expect(model.columnCount() == 7,
           "feature comparison table must have seven required columns");
    expect(model.rowCount() == 1, "canonical feature row must be published");
    expect(model.headerData(stepcompare::gui::FeatureComparisonModel::DifferenceBMinusA,
                            Qt::Horizontal)
               .toString() == QStringLiteral("Sai lệch B - A"),
           "feature difference header must state B-A convention");
    const QString difference =
        model.data(model.index(0, stepcompare::gui::FeatureComparisonModel::DifferenceBMinusA))
            .toString();
    expect(difference.contains(QStringLiteral("ABS Δ")) &&
               difference.contains(QStringLiteral("ALIGNED Δ")),
           "feature row must distinguish absolute and aligned differences");
    const QString result =
        model.data(model.index(0, stepcompare::gui::FeatureComparisonModel::Result))
            .toString();
    expect(result == QStringLiteral("CHECK / FEATURE_AMBIGUOUS"),
           "ambiguous recognition must never be presented as PASS");
    const auto target = model.selectionTarget(model.index(0, 0));
    expect(target.ownerStableId == "preview/A/a-same" &&
               target.faceIndices == std::vector<std::uint32_t>{7},
           "feature selection must resolve owner and geometric faces");
}

void pairIsolationUsesCanonicalOccurrenceEvidenceOnly() {
    stepcompare::reporting::Report report;
    stepcompare::reporting::ComponentRow exact;
    exact.idA = "assembly/plate/occurrence-1";
    exact.idB = "assembly/plate/occurrence-7";
    exact.nameA = exact.nameB = "Repeated Plate";
    exact.matchStatus = "MATCH_EXACT";
    exact.confidence = 1.0;
    report.components.push_back(exact);

    auto probable = exact;
    probable.idA = "assembly/plate/occurrence-2";
    probable.idB = "assembly/plate/occurrence-8";
    probable.matchStatus = "MATCH_PROBABLE";
    probable.confidence = 0.99;
    report.components.push_back(probable);

    const auto resolved = stepcompare::gui::resolveCanonicalPair(
        report, "preview/A/assembly/plate/occurrence-1");
    expect(resolved.resolved() &&
               resolved.stableIdA ==
                   "preview/A/assembly/plate/occurrence-1" &&
               resolved.stableIdB ==
                   "preview/B/assembly/plate/occurrence-7",
           "isolate pair must use canonical occurrence IDs, not repeated Part names");

    const auto reverse = stepcompare::gui::resolveCanonicalPair(
        report, "preview/B/assembly/plate/occurrence-7");
    expect(reverse.resolved() && reverse.stableIdA == resolved.stableIdA,
           "pair lookup must be symmetric from a selected B occurrence");
    expect(stepcompare::gui::occurrenceNodeId(resolved.stableIdA) ==
               "assembly/plate/occurrence-1" &&
               stepcompare::gui::occurrenceNodeId(resolved.stableIdB) ==
                   "assembly/plate/occurrence-7",
           "feature pair request must preserve the exact occurrence path after removing only the viewer prefix");

    const auto rejectedProbable = stepcompare::gui::resolveCanonicalPair(
        report, "preview/A/assembly/plate/occurrence-2");
    expect(rejectedProbable.status ==
               stepcompare::gui::PairResolutionStatus::MatchAmbiguous,
           "MATCH_PROBABLE must fail closed even at high numeric confidence");

    const auto root = stepcompare::gui::resolveCanonicalPair(report, "preview/A");
    expect(root.status == stepcompare::gui::PairResolutionStatus::NotAnOccurrence,
           "assembly roots must not be treated as occurrence pair evidence");
}

void droppedStepFilesPreserveExistingComparison() {
    using stepcompare::gui::DroppedStepOpenTarget;
    using stepcompare::gui::planDroppedStepFiles;

    const auto emptyA = planDroppedStepFiles(
        {QStringLiteral("old.step")}, false, false, false);
    expect(emptyA.target == DroppedStepOpenTarget::CurrentA,
           "first dropped STEP must populate File A");

    const auto emptyB = planDroppedStepFiles(
        {QStringLiteral("new.STP")}, true, false, false);
    expect(emptyB.target == DroppedStepOpenTarget::CurrentB,
           "second dropped STEP must populate the empty File B slot");

    const auto pair = planDroppedStepFiles(
        {QStringLiteral("old.step"), QStringLiteral("new.stp")},
        false,
        false,
        false);
    expect(pair.target == DroppedStepOpenTarget::CurrentPair,
           "two dropped STEP files must form A/B in an empty window");

    const auto preserve = planDroppedStepFiles(
        {QStringLiteral("next.step")}, true, true, false);
    expect(preserve.target == DroppedStepOpenTarget::NewWindowA,
           "dropping onto a completed comparison must preserve it in the current window");

    const auto busy = planDroppedStepFiles(
        {QStringLiteral("a.step"), QStringLiteral("b.step")},
        false,
        false,
        true);
    expect(busy.target == DroppedStepOpenTarget::NewWindowPair,
           "dropping while OCCT is busy must route the pair to a new window");

    const auto rejected = planDroppedStepFiles(
        {QStringLiteral("unsafe.ipt")}, false, false, false);
    expect(!rejected.accepted(),
           "unsupported dropped formats must be rejected, never misread as STEP");
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    vietnameseOverallVerdictIsExplicit();
    parametersHaveRequiredColumnsGroupsAndFormatting();
    componentModelFiltersAndStableIdsScale();
    featureModelIsFailClosedAndShowsBothCoordinateFrames();
    pairIsolationUsesCanonicalOccurrenceEvidenceOnly();
    droppedStepFilesPreserveExistingComparison();

    if (failures != 0) {
        std::cerr << failures << " readability assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All comparison readability model tests passed\n";
    return EXIT_SUCCESS;
}
