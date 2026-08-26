#pragma once

#include "stepcompare/domain/geometry.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace stepcompare::import {

struct RigidTransformMm final {
    // Row-major homogeneous transform. Translation entries are millimetres.
    std::array<double, 16> matrix{
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    };
};

// Deliberately empty abstraction: native CAD handles stay behind adapter ports.
class GeometryPayload {
public:
    virtual ~GeometryPayload() = default;
};

using GeometryPayloadPtr = std::shared_ptr<const GeometryPayload>;

struct PartPrototype final {
    std::string id;
    std::string nameUtf8;
    domain::GeometryStatistics statistics{};
    GeometryPayloadPtr geometry;
};

struct AssemblyNode final {
    std::string id;
    std::optional<std::string> parentId;
    std::vector<std::string> childIds;
    std::optional<std::string> prototypeId;
    std::string nameUtf8;
    RigidTransformMm localTransform{};
    bool isAssembly{false};
    bool isInstance{false};
};

enum class UnitNormalizationStatus {
    NormalizedToMillimetres,
    MetadataMissing,
    Unverified,
};

struct LengthUnitInfo final {
    std::vector<std::string> sourceUnitNames;
    UnitNormalizationStatus status{UnitNormalizationStatus::Unverified};
    double targetMillimetresPerUnit{1.0};
};

struct ImportedModel final {
    std::u8string sourcePathUtf8;
    LengthUnitInfo lengthUnit{};
    std::vector<std::string> rootNodeIds;
    std::vector<AssemblyNode> nodes;
    std::vector<PartPrototype> prototypes;
};

}  // namespace stepcompare::import
