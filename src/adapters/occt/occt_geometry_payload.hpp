#pragma once

#include "stepcompare/import/imported_model.hpp"

#include <TopoDS_Shape.hxx>

#include <memory>
#include <utility>

namespace stepcompare::adapters::occt {

class OcctGeometryPayload final : public import::GeometryPayload {
public:
    explicit OcctGeometryPayload(TopoDS_Shape shape)
        : shape_(std::move(shape)) {}

    [[nodiscard]] const TopoDS_Shape& shape() const noexcept {
        return shape_;
    }

private:
    TopoDS_Shape shape_;
};

[[nodiscard]] inline const TopoDS_Shape* tryGetShape(
    const import::GeometryPayloadPtr& payload) noexcept {
    const auto native = std::dynamic_pointer_cast<const OcctGeometryPayload>(
        payload);
    return native ? &native->shape() : nullptr;
}

}  // namespace stepcompare::adapters::occt
