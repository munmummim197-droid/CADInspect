#include <stepcompare/reporting/writers.hpp>

#include <cstdlib>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#if STEPCOMPARE_TEST_QT_JSON
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#endif

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class CommaDecimalPunctuation final : public std::numpunct<char> {
protected:
    [[nodiscard]] char do_decimal_point() const override { return ','; }
};

stepcompare::reporting::Report sampleReport() {
    using namespace stepcompare::reporting;
    Report report{};
    report.schemaVersion = "1.1";
    report.softwareVersion = "0.1.0";
    report.algorithmVersion = "fast-2";
    report.inputA = {
        .pathUtf8 = "D:\\Dự án\\\"Bản vẽ\"\nChi tiết A.step",
        .sha256 = "aa00",
        .sizeBytes = 42,
        .modifiedTimeUtc = "2026-08-26T01:02:03Z",
    };
    report.inputB = {
        .pathUtf8 = "D:\\Dự án\\Chi tiết B.step",
        .sha256 = "bb11",
        .sizeBytes = 84,
        .modifiedTimeUtc = "2026-08-26T01:02:04Z",
    };
    report.tolerances.positionMm = 0.01;
    report.statisticsA.volumeMm3 = 1234.5;
    report.statisticsB.volumeMm3 = 1235.75;
    report.placement.translationBMinusAMm = {5.0, -0.01, 0.0};
    report.placement.rotationAngleDegrees = 0.5;
    report.deepDeviation = {
        .available = true,
        .maximumMm = 0.125,
        .meanMm = 0.025,
        .rmsMm = 0.04,
        .percentile95Mm = 0.1,
        .sampleCount = 128,
        .triangleDistanceEvaluations = 4096,
    };
    report.execution = {
        .status = "COMPLETED",
        .terminalPhase = "Surface deviation",
        .cancellationRequested = false,
        .allRequiredEvidenceComplete = true,
    };
    report.cache = {
        .enabled = true,
        .hit = true,
        .key = "cache-key",
        .hits = 2,
        .misses = 1,
        .evictions = 0,
        .usedBytes = 2048,
        .budgetBytes = 4096,
    };
    report.timings.push_back({"STEP import", 12.5});
    report.verdict = {"FAIL", {"SAME_GEOMETRY_POSITION_CHANGED"}};
    report.components.push_back({
        .idA = "A-1",
        .idB = "B-1",
        .nameA = "Tấm, \"trên\"\r\ntrái",
        .nameB = "Tấm trên",
        .matchStatus = "MATCH_GEOMETRY",
        .geometryStatus = "SAME",
        .positionStatus = "MOVED",
        .translationBMinusAMm = {5.0, -0.01, 0.0},
        .rotationAngleDegrees = 0.5,
        .volumeDifferenceMm3 = 1.25,
        .surfaceAreaDifferenceMm2 = -2.5,
        .deviation = {.available = true, .maximumMm = 0.125},
        .confidence = 0.875,
    });
    FeatureRow feature;
    feature.idA = "A-1/feature/7";
    feature.idB = "B-1/feature/7";
    feature.ownerComponentIdA = "A-1";
    feature.ownerComponentIdB = "B-1";
    feature.type = "THROUGH_HOLE";
    feature.evidenceStatus = "GEOMETRY_PROVEN";
    feature.result = "PASS";
    feature.reason = "FEATURE_SAME_AFTER_ALIGNMENT";
    feature.profileA = feature.profileB = "CIRCULAR";
    feature.throughA = feature.throughB = true;
    feature.confidence = 0.95;
    report.features.push_back(std::move(feature));
    return report;
}

void jsonTests() {
    const auto json = stepcompare::reporting::toJson(sampleReport());
    expect(json.starts_with('{') && json.ends_with("}\n"),
           "JSON must be one complete object with a trailing newline");
    expect(json.find("\"schemaVersion\":\"1.1\"") != std::string::npos,
           "JSON must contain schemaVersion");
    expect(json.find("\"algorithmVersion\":\"fast-2\"") != std::string::npos,
           "JSON must contain algorithmVersion");
    expect(json.find("Dự án") != std::string::npos,
           "JSON must preserve UTF-8 text");
    expect(json.find("\\\"Bản vẽ\\\"\\nChi tiết") != std::string::npos,
           "JSON must escape quotes and newlines");
    expect(json.find("\"translationBMinusAMm\":{\"x\":5,\"y\":-0.01,\"z\":0}") !=
               std::string::npos,
           "JSON must encode B-minus-A placement numerically");
    expect(json.find("1234.5") != std::string::npos,
           "JSON must use a decimal point");
    expect(json.find("\"status\":\"COMPLETED\"") != std::string::npos &&
               json.find("\"allRequiredEvidenceComplete\":true") !=
                   std::string::npos,
           "JSON must expose execution completion semantics");
    expect(json.find("\"hit\":true") != std::string::npos &&
               json.find("\"key\":\"cache-key\"") != std::string::npos,
           "JSON must expose cache evidence");
    expect(json.find("\"sampleCount\":128") != std::string::npos &&
               json.find("\"triangleDistanceEvaluations\":4096") !=
                   std::string::npos,
           "JSON must expose quantitative deviation evidence counts");
#if STEPCOMPARE_TEST_QT_JSON
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(
        QByteArray(json.data(), static_cast<qsizetype>(json.size())),
        &parseError);
    expect(parseError.error == QJsonParseError::NoError && parsed.isObject(),
           "canonical report must be syntactically parsed as a JSON object");
    if (parsed.isObject()) {
        const QJsonObject root = parsed.object();
        expect(root.value(QStringLiteral("schemaVersion")).toString() ==
                   QStringLiteral("1.1"),
               "parsed canonical schema version must be 1.1");
        expect(root.value(QStringLiteral("features")).isArray() &&
                   root.value(QStringLiteral("features")).toArray().size() == 1,
               "parsed 1.1 canonical report must carry additive feature evidence");
        expect(root.value(QStringLiteral("verdict")).toObject()
                       .value(QStringLiteral("decision"))
                       .toString() == QStringLiteral("FAIL") &&
                   root.value(QStringLiteral("placement")).toObject()
                       .value(QStringLiteral("translationBMinusAMm")).toObject()
                       .value(QStringLiteral("x")).toDouble() == 5.0,
               "legacy 1.0 field meanings must remain parseable and unchanged in 1.1");
    }
#else
    expect(json.starts_with('{') && json.ends_with("}\n"),
           "standalone writer configuration must still emit a complete JSON object");
#endif
}

void csvTests() {
    const auto csv = stepcompare::reporting::toCsv(sampleReport());
    expect(csv.ends_with("\r\n"), "CSV records must use CRLF endings");
    bool hasBareLf = false;
    for (std::size_t index = 0; index < csv.size(); ++index) {
        if (csv[index] == '\n' && (index == 0 || csv[index - 1] != '\r')) {
            hasBareLf = true;
        }
    }
    expect(!hasBareLf, "CSV must not emit bare LF line endings");
    expect(csv.find("\"Tấm, \"\"trên\"\"\r\ntrái\"") != std::string::npos,
           "CSV must quote commas/newlines and double embedded quotes");
    expect(csv.find("metadata,schemaVersion,1.1") != std::string::npos,
           "CSV must include versioned metadata");
    expect(csv.find("component,,,A-1,B-1") != std::string::npos,
           "CSV must include component records");
    expect(csv.find("metadata,execution.status,COMPLETED") != std::string::npos &&
               csv.find("metadata,execution.allRequiredEvidenceComplete,true") !=
                   std::string::npos,
           "CSV must expose execution completion semantics");
    expect(csv.find("metadata,cache.hit,true") != std::string::npos &&
               csv.find("metadata,cache.key,cache-key") != std::string::npos,
           "CSV must expose cache evidence");
    expect(csv.find("metadata,deepDeviation.sampleCount,128") != std::string::npos,
           "CSV must expose deviation sample count");
}

void localeInvariantTests() {
    const std::locale commaLocale(std::locale::classic(),
                                  new CommaDecimalPunctuation());
    const auto previous = std::locale();
    std::locale::global(commaLocale);
    const auto json = stepcompare::reporting::toJson(sampleReport());
    const auto csv = stepcompare::reporting::toCsv(sampleReport());
    std::locale::global(previous);

    expect(json.find("1234.5") != std::string::npos &&
               json.find("1234,5") == std::string::npos,
           "JSON numbers must ignore the process locale");
    expect(csv.find("statisticsA.volumeMm3,1234.5") != std::string::npos,
           "CSV numbers must ignore the process locale");
}

void nonFiniteTests() {
    auto report = sampleReport();
    report.statisticsA.volumeMm3 =
        std::numeric_limits<double>::infinity();
    bool jsonRejected = false;
    try {
        static_cast<void>(stepcompare::reporting::toJson(report));
    } catch (const std::invalid_argument&) {
        jsonRejected = true;
    }
    bool csvRejected = false;
    try {
        static_cast<void>(stepcompare::reporting::toCsv(report));
    } catch (const std::invalid_argument&) {
        csvRejected = true;
    }
    expect(jsonRejected, "JSON writer must reject non-finite numbers");
    expect(csvRejected, "CSV writer must reject non-finite numbers");
}

}  // namespace

int main() {
    jsonTests();
    csvTests();
    localeInvariantTests();
    nonFiniteTests();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All reporting tests passed\n";
    return EXIT_SUCCESS;
}
