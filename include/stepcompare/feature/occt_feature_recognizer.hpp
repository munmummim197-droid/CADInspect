#pragma once

#include "stepcompare/feature/feature_recognition_port.hpp"

namespace stepcompare::feature {

class OcctFeatureRecognizer final : public FeatureRecognitionPort {
public:
    [[nodiscard]] FeatureRecognitionResult recognize(
        const import::GeometryPayloadPtr& geometry,
        double linearToleranceMm,
        double angularToleranceDegrees,
        std::stop_token cancellation = {}) noexcept override;
};

}  // namespace stepcompare::feature
