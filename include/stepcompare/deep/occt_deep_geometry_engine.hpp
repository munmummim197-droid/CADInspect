#pragma once

#include "stepcompare/deep/deep_geometry_port.hpp"

namespace stepcompare::deep {

class OcctDeepGeometryEngine final : public DeepGeometryPort {
public:
    [[nodiscard]] DeepGeometryResult compareAligned(
        const DeepGeometryRequest& request) noexcept override;
};

}  // namespace stepcompare::deep
