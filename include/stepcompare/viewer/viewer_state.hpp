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

enum class PresentationMode {
    Shaded,
    ShadedWithEdges,
    Wireframe,
    TransparentXRay,
    Section,
};

enum class SectionDirection {
    XY,
    YZ,
    ZX,
    Front,
    Top,
    Right,
    Camera,
};

enum class SectionTarget {
    A,
    B,
    Both,
};

struct SectionSettings final {
    SectionDirection direction{SectionDirection::Camera};
    SectionTarget target{SectionTarget::Both};
    // Normalized within the selected model bounds: -1.0 .. +1.0.
    double normalizedOffset{};
    bool flipped{};

    friend bool operator==(const SectionSettings&, const SectionSettings&) = default;
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
    [[nodiscard]] PresentationMode presentationMode() const noexcept;
    [[nodiscard]] const SectionSettings& sectionSettings() const noexcept;
    [[nodiscard]] const std::optional<StableSelectionId>& selection() const noexcept;

    void setLayer(SceneLayer layer) noexcept;
    void setCoordinates(CoordinateMode coordinates) noexcept;
    void setOrientation(CameraOrientation orientation) noexcept;
    void setPresentationMode(PresentationMode mode) noexcept;
    void setSectionSettings(SectionSettings settings) noexcept;
    void resetSectionSettings() noexcept;
    void select(StableSelectionId stableId);
    void clearSelection() noexcept;

    [[nodiscard]] LayerVisibility visibility() const noexcept;
    [[nodiscard]] std::string_view coordinateBanner() const noexcept;

private:
    SceneLayer layer_{SceneLayer::Overlay};
    CoordinateMode coordinates_{CoordinateMode::Absolute};
    CameraOrientation orientation_{CameraOrientation::Isometric};
    PresentationMode presentationMode_{PresentationMode::ShadedWithEdges};
    SectionSettings sectionSettings_{};
    std::optional<StableSelectionId> selection_;
};

[[nodiscard]] std::string_view toString(SceneLayer layer) noexcept;
[[nodiscard]] std::string_view toString(CoordinateMode coordinates) noexcept;
[[nodiscard]] std::string_view toString(PresentationMode mode) noexcept;
[[nodiscard]] std::string_view toString(SectionDirection direction) noexcept;
[[nodiscard]] std::string_view toString(SectionTarget target) noexcept;

}  // namespace stepcompare::viewer
