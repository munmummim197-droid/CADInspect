#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace stepcompare::viewer {

enum class SceneLayer {
    AOnly,
    BOnly,
    Overlay,
    Difference,
};

enum class CoordinateMode {
    Absolute,
    Aligned,
};

enum class ModelSide {
    A,
    B,
};

enum class CameraOrientation {
    Front,
    Back,
    Left,
    Right,
    Top,
    Bottom,
    Isometric,
};

class StableSelectionId final {
public:
    StableSelectionId() = delete;
    explicit StableSelectionId(std::string value);

    [[nodiscard]] const std::string& value() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    friend bool operator==(const StableSelectionId&, const StableSelectionId&) = default;

private:
    std::string value_;
};

struct LayerVisibility final {
    bool showA{};
    bool showB{};
    bool differencesOnly{};
};

class ViewerStateModel final {
public:
    [[nodiscard]] SceneLayer layer() const noexcept;
    [[nodiscard]] CoordinateMode coordinates() const noexcept;
    [[nodiscard]] CameraOrientation orientation() const noexcept;
    [[nodiscard]] const std::optional<StableSelectionId>& selection() const noexcept;

    void setLayer(SceneLayer layer) noexcept;
    void setCoordinates(CoordinateMode coordinates) noexcept;
    void setOrientation(CameraOrientation orientation) noexcept;
    void select(StableSelectionId stableId);
    void clearSelection() noexcept;

    [[nodiscard]] LayerVisibility visibility() const noexcept;
    [[nodiscard]] std::string_view coordinateBanner() const noexcept;

private:
    SceneLayer layer_{SceneLayer::Overlay};
    CoordinateMode coordinates_{CoordinateMode::Absolute};
    CameraOrientation orientation_{CameraOrientation::Isometric};
    std::optional<StableSelectionId> selection_;
};

[[nodiscard]] std::string_view toString(SceneLayer layer) noexcept;
[[nodiscard]] std::string_view toString(CoordinateMode coordinates) noexcept;

}  // namespace stepcompare::viewer
