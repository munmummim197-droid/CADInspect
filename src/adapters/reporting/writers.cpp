#include <stepcompare/reporting/writers.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stepcompare::reporting {
namespace {

[[nodiscard]] std::string number(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Reports cannot contain non-finite numbers");
    }

    std::array<char, 64> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                                      value, std::chars_format::general,
                                      std::numeric_limits<double>::max_digits10);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("Unable to format report number");
    }
    return {buffer.data(), result.ptr};
}

[[nodiscard]] std::string unsignedNumber(std::uint64_t value) {
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                                      value);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("Unable to format report integer");
    }
    return {buffer.data(), result.ptr};
}

void appendJsonString(std::string& output, std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const auto rawCharacter : value) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        switch (character) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (character < 0x20U) {
                output += "\\u00";
                output.push_back(hex[character >> 4U]);
                output.push_back(hex[character & 0x0fU]);
            } else {
                output.push_back(rawCharacter);
            }
        }
    }
    output.push_back('"');
}

void appendKey(std::string& output, std::string_view key) {
    appendJsonString(output, key);
    output.push_back(':');
}

void appendStringMember(std::string& output, std::string_view key,
                        std::string_view value, bool comma = true) {
    appendKey(output, key);
    appendJsonString(output, value);
    if (comma) {
        output.push_back(',');
    }
}

void appendNumberMember(std::string& output, std::string_view key, double value,
                        bool comma = true) {
    appendKey(output, key);
    output += number(value);
    if (comma) {
        output.push_back(',');
    }
}

void appendUnsignedMember(std::string& output, std::string_view key,
                          std::uint64_t value, bool comma = true) {
    appendKey(output, key);
    output += unsignedNumber(value);
    if (comma) {
        output.push_back(',');
    }
}

void appendBoolMember(std::string& output, std::string_view key, bool value,
                      bool comma = true) {
    appendKey(output, key);
    output += value ? "true" : "false";
    if (comma) {
        output.push_back(',');
    }
}

void appendVector(std::string& output, const Vector3& value) {
    output.push_back('{');
    appendNumberMember(output, "x", value.x);
    appendNumberMember(output, "y", value.y);
    appendNumberMember(output, "z", value.z, false);
    output.push_back('}');
}

void appendQuaternion(std::string& output, const Quaternion& value) {
    output.push_back('{');
    appendNumberMember(output, "w", value.w);
    appendNumberMember(output, "x", value.x);
    appendNumberMember(output, "y", value.y);
    appendNumberMember(output, "z", value.z, false);
    output.push_back('}');
}

void appendInput(std::string& output, const InputIdentity& input) {
    output.push_back('{');
    appendStringMember(output, "path", input.pathUtf8);
    appendStringMember(output, "sha256", input.sha256);
    appendUnsignedMember(output, "sizeBytes", input.sizeBytes);
    appendStringMember(output, "modifiedTimeUtc", input.modifiedTimeUtc, false);
    output.push_back('}');
}

void appendTolerances(std::string& output, const Tolerances& tolerances) {
    output.push_back('{');
    appendNumberMember(output, "positionMm", tolerances.positionMm);
    appendNumberMember(output, "surfaceMm", tolerances.surfaceMm);
    appendNumberMember(output, "angularDegrees", tolerances.angularDegrees);
    appendNumberMember(output, "booleanFuzzyMm", tolerances.booleanFuzzyMm);
    appendNumberMember(output, "relativeProperty", tolerances.relativeProperty,
                       false);
    output.push_back('}');
}

void appendGeometryStatistics(std::string& output,
                              const GeometryStatistics& statistics) {
    output.push_back('{');
    appendKey(output, "boundingBoxMinimumMm");
    appendVector(output, statistics.boundingBoxMinimumMm);
    output.push_back(',');
    appendKey(output, "boundingBoxMaximumMm");
    appendVector(output, statistics.boundingBoxMaximumMm);
    output.push_back(',');
    appendKey(output, "sizeMm");
    appendVector(output, statistics.sizeMm);
    output.push_back(',');
    appendNumberMember(output, "volumeMm3", statistics.volumeMm3);
    appendNumberMember(output, "surfaceAreaMm2", statistics.surfaceAreaMm2);
    appendKey(output, "centerOfMassMm");
    appendVector(output, statistics.centerOfMassMm);
    output.push_back(',');
    appendUnsignedMember(output, "solidCount", statistics.solidCount);
    appendUnsignedMember(output, "shellCount", statistics.shellCount);
    appendUnsignedMember(output, "faceCount", statistics.faceCount);
    appendUnsignedMember(output, "edgeCount", statistics.edgeCount);
    appendUnsignedMember(output, "vertexCount", statistics.vertexCount);
    appendKey(output, "principalMoments");
    appendVector(output, statistics.principalMoments);
    output.push_back(',');
    appendKey(output, "principalAxes");
    output.push_back('[');
    for (std::size_t index = 0; index < statistics.principalAxes.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        appendVector(output, statistics.principalAxes[index]);
    }
    output += "]}";
}

void appendPlacement(std::string& output, const PlacementResult& placement) {
    output.push_back('{');
    appendKey(output, "translationBMinusAMm");
    appendVector(output, placement.translationBMinusAMm);
    output.push_back(',');
    appendKey(output, "rotationBToA");
    appendQuaternion(output, placement.rotationBToA);
    output.push_back(',');
    appendKey(output, "displayEulerDegrees");
    appendVector(output, placement.displayEulerDegrees);
    output.push_back(',');
    appendNumberMember(output, "rotationAngleDegrees",
                       placement.rotationAngleDegrees);
    appendBoolMember(output, "ambiguousBySymmetry",
                     placement.ambiguousBySymmetry, false);
    output.push_back('}');
}

void appendDeviation(std::string& output,
                     const DeviationStatistics& deviation) {
    output.push_back('{');
    appendBoolMember(output, "available", deviation.available);
    appendNumberMember(output, "maximumMm", deviation.maximumMm);
    appendNumberMember(output, "meanMm", deviation.meanMm);
    appendNumberMember(output, "rmsMm", deviation.rmsMm);
    appendNumberMember(output, "percentile95Mm", deviation.percentile95Mm);
    appendUnsignedMember(output, "sampleCount", deviation.sampleCount);
    appendUnsignedMember(output, "triangleDistanceEvaluations",
                         deviation.triangleDistanceEvaluations, false);
    output.push_back('}');
}

void appendExecution(std::string& output,
                     const ExecutionMetadata& execution) {
    output.push_back('{');
    appendStringMember(output, "status", execution.status);
    appendStringMember(output, "terminalPhase", execution.terminalPhase);
    appendBoolMember(output, "cancellationRequested",
                     execution.cancellationRequested);
    appendBoolMember(output, "allRequiredEvidenceComplete",
                     execution.allRequiredEvidenceComplete, false);
    output.push_back('}');
}

void appendCache(std::string& output, const CacheMetadata& cache) {
    output.push_back('{');
    appendBoolMember(output, "enabled", cache.enabled);
    appendBoolMember(output, "hit", cache.hit);
    appendStringMember(output, "key", cache.key);
    appendUnsignedMember(output, "hits", cache.hits);
    appendUnsignedMember(output, "misses", cache.misses);
    appendUnsignedMember(output, "evictions", cache.evictions);
    appendUnsignedMember(output, "usedBytes", cache.usedBytes);
    appendUnsignedMember(output, "budgetBytes", cache.budgetBytes, false);
    output.push_back('}');
}

void appendComponent(std::string& output, const ComponentRow& component) {
    output.push_back('{');
    appendStringMember(output, "idA", component.idA);
    appendStringMember(output, "idB", component.idB);
    appendStringMember(output, "nameA", component.nameA);
    appendStringMember(output, "nameB", component.nameB);
    appendStringMember(output, "matchStatus", component.matchStatus);
    appendStringMember(output, "geometryStatus", component.geometryStatus);
    appendStringMember(output, "positionStatus", component.positionStatus);
    appendKey(output, "translationBMinusAMm");
    appendVector(output, component.translationBMinusAMm);
    output.push_back(',');
    appendKey(output, "rotationBToA");
    appendQuaternion(output, component.rotationBToA);
    output.push_back(',');
    appendNumberMember(output, "rotationAngleDegrees",
                       component.rotationAngleDegrees);
    appendKey(output, "boundingBoxSizeDifferenceMm");
    appendVector(output, component.boundingBoxSizeDifferenceMm);
    output.push_back(',');
    appendNumberMember(output, "volumeDifferenceMm3",
                       component.volumeDifferenceMm3);
    appendNumberMember(output, "surfaceAreaDifferenceMm2",
                       component.surfaceAreaDifferenceMm2);
    appendKey(output, "deviation");
    appendDeviation(output, component.deviation);
    output.push_back(',');
    appendNumberMember(output, "confidence", component.confidence, false);
    output.push_back('}');
}

void appendUnsignedArray(std::string& output,
                         const std::vector<std::uint32_t>& values) {
    output.push_back('[');
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        output += unsignedNumber(values[index]);
    }
    output.push_back(']');
}

void appendFeature(std::string& output, const FeatureRow& feature) {
    output.push_back('{');
    appendStringMember(output, "idA", feature.idA);
    appendStringMember(output, "idB", feature.idB);
    appendStringMember(output, "ownerComponentIdA", feature.ownerComponentIdA);
    appendStringMember(output, "ownerComponentIdB", feature.ownerComponentIdB);
    appendStringMember(output, "type", feature.type);
    appendStringMember(output, "evidenceStatus", feature.evidenceStatus);
    appendStringMember(output, "result", feature.result);
    appendStringMember(output, "reason", feature.reason);
    appendKey(output, "centerAAbsoluteMm");
    appendVector(output, feature.centerAAbsoluteMm);
    output.push_back(',');
    appendKey(output, "centerBAbsoluteMm");
    appendVector(output, feature.centerBAbsoluteMm);
    output.push_back(',');
    appendKey(output, "centerBAlignedMm");
    appendVector(output, feature.centerBAlignedMm);
    output.push_back(',');
    appendKey(output, "absoluteDifferenceBMinusAMm");
    appendVector(output, feature.absoluteDifferenceBMinusAMm);
    output.push_back(',');
    appendKey(output, "alignedDifferenceBMinusAMm");
    appendVector(output, feature.alignedDifferenceBMinusAMm);
    output.push_back(',');
    appendKey(output, "axisA");
    appendVector(output, feature.axisA);
    output.push_back(',');
    appendKey(output, "axisB");
    appendVector(output, feature.axisB);
    output.push_back(',');
    appendKey(output, "axisBAligned");
    appendVector(output, feature.axisBAligned);
    output.push_back(',');
    appendNumberMember(output, "primarySizeAMm", feature.primarySizeAMm);
    appendNumberMember(output, "primarySizeBMm", feature.primarySizeBMm);
    appendNumberMember(output, "secondarySizeAMm", feature.secondarySizeAMm);
    appendNumberMember(output, "secondarySizeBMm", feature.secondarySizeBMm);
    appendNumberMember(output, "depthAMm", feature.depthAMm);
    appendNumberMember(output, "depthBMm", feature.depthBMm);
    appendNumberMember(output, "radiusAMm", feature.radiusAMm);
    appendNumberMember(output, "radiusBMm", feature.radiusBMm);
    appendNumberMember(output, "angleADegrees", feature.angleADegrees);
    appendNumberMember(output, "angleBDegrees", feature.angleBDegrees);
    appendStringMember(output, "profileA", feature.profileA);
    appendStringMember(output, "profileB", feature.profileB);
    appendBoolMember(output, "throughA", feature.throughA);
    appendBoolMember(output, "throughB", feature.throughB);
    appendNumberMember(output, "positionToleranceMm",
                       feature.positionToleranceMm);
    appendNumberMember(output, "angularToleranceDegrees",
                       feature.angularToleranceDegrees);
    appendNumberMember(output, "confidence", feature.confidence);
    appendKey(output, "faceIndicesA");
    appendUnsignedArray(output, feature.faceIndicesA);
    output.push_back(',');
    appendKey(output, "faceIndicesB");
    appendUnsignedArray(output, feature.faceIndicesB);
    output.push_back('}');
}

[[nodiscard]] std::string makeJson(const Report& report) {
    std::string output;
    output.reserve(2048U + report.components.size() * 512U +
                   report.features.size() * 1024U);
    output.push_back('{');
    appendStringMember(output, "schemaVersion", report.schemaVersion);
    appendStringMember(output, "softwareVersion", report.softwareVersion);
    appendStringMember(output, "algorithmVersion", report.algorithmVersion);
    appendKey(output, "inputA");
    appendInput(output, report.inputA);
    output.push_back(',');
    appendKey(output, "inputB");
    appendInput(output, report.inputB);
    output.push_back(',');
    appendKey(output, "tolerances");
    appendTolerances(output, report.tolerances);
    output.push_back(',');
    appendKey(output, "execution");
    appendExecution(output, report.execution);
    output.push_back(',');
    appendKey(output, "cache");
    appendCache(output, report.cache);
    output.push_back(',');
    appendKey(output, "statisticsA");
    appendGeometryStatistics(output, report.statisticsA);
    output.push_back(',');
    appendKey(output, "statisticsB");
    appendGeometryStatistics(output, report.statisticsB);
    output.push_back(',');
    appendKey(output, "placement");
    appendPlacement(output, report.placement);
    output.push_back(',');
    appendKey(output, "deepDeviation");
    appendDeviation(output, report.deepDeviation);
    output.push_back(',');
    appendKey(output, "timings");
    output.push_back('[');
    for (std::size_t index = 0; index < report.timings.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        output.push_back('{');
        appendStringMember(output, "phase", report.timings[index].phase);
        appendNumberMember(output, "elapsedMilliseconds",
                           report.timings[index].elapsedMilliseconds, false);
        output.push_back('}');
    }
    output += "],";
    appendKey(output, "verdict");
    output.push_back('{');
    appendStringMember(output, "decision", report.verdict.decision);
    appendKey(output, "reasons");
    output.push_back('[');
    for (std::size_t index = 0; index < report.verdict.reasons.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        appendJsonString(output, report.verdict.reasons[index]);
    }
    output += "]},";
    appendKey(output, "components");
    output.push_back('[');
    for (std::size_t index = 0; index < report.components.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        appendComponent(output, report.components[index]);
    }
    output += "],";
    appendKey(output, "features");
    output.push_back('[');
    for (std::size_t index = 0; index < report.features.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        appendFeature(output, report.features[index]);
    }
    output += "]}\n";
    return output;
}

[[nodiscard]] std::string normalizeCsvNewlines(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '\r') {
            normalized += "\r\n";
            if (index + 1 < value.size() && value[index + 1] == '\n') {
                ++index;
            }
        } else if (value[index] == '\n') {
            normalized += "\r\n";
        } else {
            normalized.push_back(value[index]);
        }
    }
    return normalized;
}

void appendCsvField(std::string& output, std::string_view rawValue) {
    const auto value = normalizeCsvNewlines(rawValue);
    const bool quote = value.find_first_of(",\"\r\n") != std::string::npos;
    if (!quote) {
        output += value;
        return;
    }

    output.push_back('"');
    for (const char character : value) {
        if (character == '"') {
            output += "\"\"";
        } else {
            output.push_back(character);
        }
    }
    output.push_back('"');
}

void appendCsvRecord(std::string& output,
                     const std::vector<std::string>& fields) {
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        appendCsvField(output, fields[index]);
    }
    output += "\r\n";
}

constexpr std::size_t csvColumnCount = 28;

void appendMetadata(std::string& output, std::string key, std::string value,
                    std::string recordType = "metadata") {
    std::vector<std::string> fields(csvColumnCount);
    fields[0] = std::move(recordType);
    fields[1] = std::move(key);
    fields[2] = std::move(value);
    appendCsvRecord(output, fields);
}

void appendMetadata(std::string& output, std::string key, double value,
                    std::string recordType = "metadata") {
    appendMetadata(output, std::move(key), number(value),
                   std::move(recordType));
}

void appendMetadata(std::string& output, std::string key, std::uint64_t value,
                    std::string recordType = "metadata") {
    appendMetadata(output, std::move(key), unsignedNumber(value),
                   std::move(recordType));
}

void appendVectorMetadata(std::string& output, std::string_view prefix,
                          const Vector3& value) {
    appendMetadata(output, std::string(prefix) + ".x", value.x);
    appendMetadata(output, std::string(prefix) + ".y", value.y);
    appendMetadata(output, std::string(prefix) + ".z", value.z);
}

void appendStatisticsMetadata(std::string& output, std::string_view prefix,
                              const GeometryStatistics& statistics) {
    appendVectorMetadata(output, std::string(prefix) + ".boundingBoxMinimumMm",
                         statistics.boundingBoxMinimumMm);
    appendVectorMetadata(output, std::string(prefix) + ".boundingBoxMaximumMm",
                         statistics.boundingBoxMaximumMm);
    appendVectorMetadata(output, std::string(prefix) + ".sizeMm",
                         statistics.sizeMm);
    appendMetadata(output, std::string(prefix) + ".volumeMm3",
                   statistics.volumeMm3);
    appendMetadata(output, std::string(prefix) + ".surfaceAreaMm2",
                   statistics.surfaceAreaMm2);
    appendVectorMetadata(output, std::string(prefix) + ".centerOfMassMm",
                         statistics.centerOfMassMm);
    appendMetadata(output, std::string(prefix) + ".solidCount",
                   statistics.solidCount);
    appendMetadata(output, std::string(prefix) + ".shellCount",
                   statistics.shellCount);
    appendMetadata(output, std::string(prefix) + ".faceCount",
                   statistics.faceCount);
    appendMetadata(output, std::string(prefix) + ".edgeCount",
                   statistics.edgeCount);
    appendMetadata(output, std::string(prefix) + ".vertexCount",
                   statistics.vertexCount);
    appendVectorMetadata(output, std::string(prefix) + ".principalMoments",
                         statistics.principalMoments);
    for (std::size_t index = 0; index < statistics.principalAxes.size(); ++index) {
        appendVectorMetadata(output,
                             std::string(prefix) + ".principalAxes." +
                                 unsignedNumber(index),
                             statistics.principalAxes[index]);
    }
}

[[nodiscard]] std::string makeCsv(const Report& report) {
    std::string output;
    appendCsvRecord(output,
                    {"recordType", "key", "value", "componentIdA",
                     "componentIdB", "componentNameA", "componentNameB",
                     "matchStatus", "geometryStatus", "positionStatus",
                     "deltaXmm", "deltaYmm", "deltaZmm",
                     "rotationAngleDegrees", "volumeDifferenceMm3",
                     "surfaceAreaDifferenceMm2", "maximumDeviationMm",
                     "meanDeviationMm", "rmsDeviationMm",
                     "percentile95DeviationMm", "confidence", "rotationW",
                     "rotationX", "rotationY", "rotationZ",
                     "bboxSizeDifferenceXmm", "bboxSizeDifferenceYmm",
                     "bboxSizeDifferenceZmm"});

    appendMetadata(output, "schemaVersion", report.schemaVersion);
    appendMetadata(output, "softwareVersion", report.softwareVersion);
    appendMetadata(output, "algorithmVersion", report.algorithmVersion);
    appendMetadata(output, "inputA.path", report.inputA.pathUtf8);
    appendMetadata(output, "inputA.sha256", report.inputA.sha256);
    appendMetadata(output, "inputA.sizeBytes", report.inputA.sizeBytes);
    appendMetadata(output, "inputA.modifiedTimeUtc",
                   report.inputA.modifiedTimeUtc);
    appendMetadata(output, "inputB.path", report.inputB.pathUtf8);
    appendMetadata(output, "inputB.sha256", report.inputB.sha256);
    appendMetadata(output, "inputB.sizeBytes", report.inputB.sizeBytes);
    appendMetadata(output, "inputB.modifiedTimeUtc",
                   report.inputB.modifiedTimeUtc);
    appendMetadata(output, "tolerances.positionMm",
                   report.tolerances.positionMm);
    appendMetadata(output, "tolerances.surfaceMm", report.tolerances.surfaceMm);
    appendMetadata(output, "tolerances.angularDegrees",
                   report.tolerances.angularDegrees);
    appendMetadata(output, "tolerances.booleanFuzzyMm",
                   report.tolerances.booleanFuzzyMm);
    appendMetadata(output, "tolerances.relativeProperty",
                   report.tolerances.relativeProperty);
    appendMetadata(output, "execution.status", report.execution.status);
    appendMetadata(output, "execution.terminalPhase",
                   report.execution.terminalPhase);
    appendMetadata(output, "execution.cancellationRequested",
                   report.execution.cancellationRequested ? "true" : "false");
    appendMetadata(output, "execution.allRequiredEvidenceComplete",
                   report.execution.allRequiredEvidenceComplete ? "true" : "false");
    appendMetadata(output, "cache.enabled", report.cache.enabled ? "true" : "false");
    appendMetadata(output, "cache.hit", report.cache.hit ? "true" : "false");
    appendMetadata(output, "cache.key", report.cache.key);
    appendMetadata(output, "cache.hits", report.cache.hits);
    appendMetadata(output, "cache.misses", report.cache.misses);
    appendMetadata(output, "cache.evictions", report.cache.evictions);
    appendMetadata(output, "cache.usedBytes", report.cache.usedBytes);
    appendMetadata(output, "cache.budgetBytes", report.cache.budgetBytes);
    appendStatisticsMetadata(output, "statisticsA", report.statisticsA);
    appendStatisticsMetadata(output, "statisticsB", report.statisticsB);
    appendVectorMetadata(output, "placement.translationBMinusAMm",
                         report.placement.translationBMinusAMm);
    appendMetadata(output, "placement.rotationBToA.w",
                   report.placement.rotationBToA.w);
    appendMetadata(output, "placement.rotationBToA.x",
                   report.placement.rotationBToA.x);
    appendMetadata(output, "placement.rotationBToA.y",
                   report.placement.rotationBToA.y);
    appendMetadata(output, "placement.rotationBToA.z",
                   report.placement.rotationBToA.z);
    appendVectorMetadata(output, "placement.displayEulerDegrees",
                         report.placement.displayEulerDegrees);
    appendMetadata(output, "placement.rotationAngleDegrees",
                   report.placement.rotationAngleDegrees);
    appendMetadata(output, "placement.ambiguousBySymmetry",
                   report.placement.ambiguousBySymmetry ? "true" : "false");
    appendMetadata(output, "deepDeviation.available",
                   report.deepDeviation.available ? "true" : "false");
    appendMetadata(output, "deepDeviation.maximumMm",
                   report.deepDeviation.maximumMm);
    appendMetadata(output, "deepDeviation.meanMm", report.deepDeviation.meanMm);
    appendMetadata(output, "deepDeviation.rmsMm", report.deepDeviation.rmsMm);
    appendMetadata(output, "deepDeviation.percentile95Mm",
                   report.deepDeviation.percentile95Mm);
    appendMetadata(output, "deepDeviation.sampleCount",
                   report.deepDeviation.sampleCount);
    appendMetadata(output, "deepDeviation.triangleDistanceEvaluations",
                   report.deepDeviation.triangleDistanceEvaluations);
    for (const auto& timing : report.timings) {
        appendMetadata(output, timing.phase, timing.elapsedMilliseconds, "timing");
    }
    appendMetadata(output, "decision", report.verdict.decision, "verdict");
    for (const auto& reason : report.verdict.reasons) {
        appendMetadata(output, "reason", reason, "verdict");
    }

    for (const auto& component : report.components) {
        std::vector<std::string> fields(csvColumnCount);
        fields[0] = "component";
        fields[3] = component.idA;
        fields[4] = component.idB;
        fields[5] = component.nameA;
        fields[6] = component.nameB;
        fields[7] = component.matchStatus;
        fields[8] = component.geometryStatus;
        fields[9] = component.positionStatus;
        fields[10] = number(component.translationBMinusAMm.x);
        fields[11] = number(component.translationBMinusAMm.y);
        fields[12] = number(component.translationBMinusAMm.z);
        fields[13] = number(component.rotationAngleDegrees);
        fields[14] = number(component.volumeDifferenceMm3);
        fields[15] = number(component.surfaceAreaDifferenceMm2);
        fields[16] = number(component.deviation.maximumMm);
        fields[17] = number(component.deviation.meanMm);
        fields[18] = number(component.deviation.rmsMm);
        fields[19] = number(component.deviation.percentile95Mm);
        fields[20] = number(component.confidence);
        fields[21] = number(component.rotationBToA.w);
        fields[22] = number(component.rotationBToA.x);
        fields[23] = number(component.rotationBToA.y);
        fields[24] = number(component.rotationBToA.z);
        fields[25] = number(component.boundingBoxSizeDifferenceMm.x);
        fields[26] = number(component.boundingBoxSizeDifferenceMm.y);
        fields[27] = number(component.boundingBoxSizeDifferenceMm.z);
        appendCsvRecord(output, fields);
    }
    for (const auto& feature : report.features) {
        std::vector<std::string> fields(csvColumnCount);
        fields[0] = "feature";
        fields[1] = feature.idA + "|" + feature.idB;
        fields[2] = feature.type;
        fields[3] = feature.ownerComponentIdA;
        fields[4] = feature.ownerComponentIdB;
        fields[7] = feature.result;
        fields[8] = feature.evidenceStatus;
        fields[9] = feature.reason;
        fields[10] = number(feature.alignedDifferenceBMinusAMm.x);
        fields[11] = number(feature.alignedDifferenceBMinusAMm.y);
        fields[12] = number(feature.alignedDifferenceBMinusAMm.z);
        fields[13] = number(feature.angleBDegrees - feature.angleADegrees);
        fields[14] = number(feature.primarySizeBMm - feature.primarySizeAMm);
        fields[15] = number(feature.depthBMm - feature.depthAMm);
        fields[20] = number(feature.confidence);
        appendCsvRecord(output, fields);
    }
    return output;
}

void writeAll(std::ostream& output, const std::string& contents) {
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output) {
        throw std::ios_base::failure("Unable to write report");
    }
}

}  // namespace

void writeJson(const Report& report, std::ostream& output) {
    writeAll(output, makeJson(report));
}

std::string toJson(const Report& report) { return makeJson(report); }

void writeCsv(const Report& report, std::ostream& output) {
    writeAll(output, makeCsv(report));
}

std::string toCsv(const Report& report) { return makeCsv(report); }

}  // namespace stepcompare::reporting
