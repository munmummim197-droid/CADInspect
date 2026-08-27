#pragma once

#include "stepcompare/import/imported_model.hpp"

#include <cstdint>
#include <stop_token>
#include <string>
#include <vector>

namespace stepcompare::feature {

enum class FeatureType {
    ThroughHole,
    Counterbore,
    Keyway,
    Slot,
    BlindPocket,
    Fillet,
    Chamfer,
    Unknown,
};

enum class RecognitionEvidence {
    GeometryProven,
    Ambiguous,
};

struct Vector3 final {
    double x{};
    double y{};
    double z{};
};

struct RecognizedFeature final {
    std::string stableId{};
    FeatureType type{FeatureType::Unknown};
    RecognitionEvidence evidence{RecognitionEvidence::Ambiguous};
    double confidence{};
    Vector3 centerLocalMm{};
    Vector3 axis{};
    double primarySizeMm{};
    double secondarySizeMm{};
    double depthMm{};
    double radiusMm{};
    double angleDegrees{};
    std::string profile{};
    bool through{};
    std::vector<std::uint32_t> faceIndices{};
};

struct FeatureRecognitionResult final {
    bool completed{};
    bool cancelled{};
    std::vector<RecognizedFeature> features{};
    std::vector<std::string> diagnostics{};
};

class FeatureRecognitionPort {
public:
    virtual ~FeatureRecognitionPort() = default;

    [[nodiscard]] virtual FeatureRecognitionResult recognize(
        const import::GeometryPayloadPtr& geometry,
        double linearToleranceMm,
        double angularToleranceDegrees,
        std::stop_token cancellation = {}) noexcept = 0;
};

[[nodiscard]] inline const char* featureTypeName(FeatureType type) noexcept {
    switch (type) {
        case FeatureType::ThroughHole:
            return "THROUGH_HOLE";
        case FeatureType::Counterbore:
            return "COUNTERBORE";
        case FeatureType::Keyway:
            return "KEYWAY";
        case FeatureType::Slot:
            return "SLOT";
        case FeatureType::BlindPocket:
            return "BLIND_POCKET";
        case FeatureType::Fillet:
            return "FILLET";
        case FeatureType::Chamfer:
            return "CHAMFER";
        case FeatureType::Unknown:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

}  // namespace stepcompare::feature
