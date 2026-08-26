#pragma once

#include <stepcompare/deviation/surface_deviation_port.hpp>

namespace stepcompare::deviation {

class OcctSurfaceDeviationEngine final : public SurfaceDeviationPort {
public:
    [[nodiscard]] SurfaceDeviationResult compare(
        const SurfaceDeviationRequest& request) noexcept override;
};

}  // namespace stepcompare::deviation
