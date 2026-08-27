#include <stepcompare/viewer/viewer_state.hpp>

#include <stdexcept>
#include <utility>

namespace stepcompare::viewer {

StableSelectionId::StableSelectionId(std::string value) : value_(std::move(value)) {
    if (value_.empty()) {
        throw std::invalid_argument("A stable viewer selection ID cannot be empty");
    }
}

const std::string& StableSelectionId::value() const noexcept {
    return value_;
}

bool StableSelectionId::empty() const noexcept {
    return value_.empty();
}

SceneLayer ViewerStateModel::layer() const noexcept {
    return layer_;
}

CoordinateMode ViewerStateModel::coordinates() const noexcept {
    return coordinates_;
}

CameraOrientation ViewerStateModel::orientation() const noexcept {
    return orientation_;
}

PresentationMode ViewerStateModel::presentationMode() const noexcept {
    return presentationMode_;
}

const std::optional<StableSelectionId>& ViewerStateModel::selection() const noexcept {
    return selection_;
}

void ViewerStateModel::setLayer(const SceneLayer layer) noexcept {
    layer_ = layer;
}

void ViewerStateModel::setCoordinates(const CoordinateMode coordinates) noexcept {
    coordinates_ = coordinates;
}

void ViewerStateModel::setOrientation(const CameraOrientation orientation) noexcept {
    orientation_ = orientation;
}

void ViewerStateModel::setPresentationMode(const PresentationMode mode) noexcept {
    presentationMode_ = mode;
}

void ViewerStateModel::select(StableSelectionId stableId) {
    if (stableId.empty()) {
        throw std::invalid_argument("Cannot select an empty stable viewer ID");
    }
    selection_ = std::move(stableId);
}

void ViewerStateModel::clearSelection() noexcept {
    selection_.reset();
}

LayerVisibility ViewerStateModel::visibility() const noexcept {
    switch (layer_) {
        case SceneLayer::AOnly:
            return {.showA = true, .showB = false, .differencesOnly = false};
        case SceneLayer::BOnly:
            return {.showA = false, .showB = true, .differencesOnly = false};
        case SceneLayer::Overlay:
            return {.showA = true, .showB = true, .differencesOnly = false};
        case SceneLayer::Difference:
            return {.showA = true, .showB = true, .differencesOnly = true};
    }
    return {};
}

std::string_view ViewerStateModel::coordinateBanner() const noexcept {
    return coordinates_ == CoordinateMode::Absolute ? "ABSOLUTE COORDINATES"
                                                    : "ALIGNED GEOMETRY (B -> A)";
}

std::string_view toString(const SceneLayer layer) noexcept {
    switch (layer) {
        case SceneLayer::AOnly:
            return "A ONLY";
        case SceneLayer::BOnly:
            return "B ONLY";
        case SceneLayer::Overlay:
            return "A + B OVERLAY";
        case SceneLayer::Difference:
            return "DIFFERENCE";
    }
    return "UNKNOWN";
}

std::string_view toString(const CoordinateMode coordinates) noexcept {
    return coordinates == CoordinateMode::Absolute ? "ABSOLUTE" : "ALIGNED";
}

std::string_view toString(const PresentationMode mode) noexcept {
    switch (mode) {
        case PresentationMode::Shaded:
            return "SHADED";
        case PresentationMode::ShadedWithEdges:
            return "SHADED + EDGES";
        case PresentationMode::Wireframe:
            return "WIREFRAME";
        case PresentationMode::TransparentXRay:
            return "TRANSPARENT / X-RAY";
        case PresentationMode::Section:
            return "SECTION VIEW";
    }
    return "SHADED + EDGES";
}

}  // namespace stepcompare::viewer
