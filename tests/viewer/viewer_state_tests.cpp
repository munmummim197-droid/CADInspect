#include <stepcompare/viewer/viewer_state.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void defaultContract() {
    using namespace stepcompare::viewer;
    const ViewerStateModel state;
    expect(state.layer() == SceneLayer::Overlay, "default layer must be OVERLAY");
    expect(state.coordinates() == CoordinateMode::Absolute,
           "viewer must never silently align by default");
    expect(state.coordinateBanner() == "ABSOLUTE COORDINATES",
           "absolute coordinate banner must be explicit");
    expect(state.visibility().showA && state.visibility().showB,
           "overlay must show both models");
}

void allLayerModes() {
    using namespace stepcompare::viewer;
    ViewerStateModel state;

    state.setLayer(SceneLayer::AOnly);
    expect(state.visibility().showA && !state.visibility().showB,
           "A ONLY must hide B");
    expect(toString(state.layer()) == "A ONLY", "A ONLY label contract");

    state.setLayer(SceneLayer::BOnly);
    expect(!state.visibility().showA && state.visibility().showB,
           "B ONLY must hide A");

    state.setLayer(SceneLayer::Difference);
    expect(state.visibility().showA && state.visibility().showB &&
               state.visibility().differencesOnly,
           "DIFFERENCE must restrict both sides to difference presentations");
}

void coordinateAndSelectionContracts() {
    using namespace stepcompare::viewer;
    ViewerStateModel state;
    state.setCoordinates(CoordinateMode::Aligned);
    expect(state.coordinateBanner() == "ALIGNED GEOMETRY (B -> A)",
           "aligned banner must name the B-to-A direction");

    state.select(StableSelectionId{"assembly/part-42/instance-3"});
    expect(state.selection().has_value() &&
               state.selection()->value() == "assembly/part-42/instance-3",
           "stable selection ID must survive round trip for tree sync");
    state.clearSelection();
    expect(!state.selection(), "selection must clear deterministically");

    bool rejectedEmpty = false;
    try {
        static_cast<void>(StableSelectionId{""});
    } catch (const std::invalid_argument&) {
        rejectedEmpty = true;
    }
    expect(rejectedEmpty, "empty stable IDs must be rejected");
}

void cameraStateContract() {
    using namespace stepcompare::viewer;
    ViewerStateModel state;
    constexpr CameraOrientation orientations[]{
        CameraOrientation::Front,
        CameraOrientation::Back,
        CameraOrientation::Left,
        CameraOrientation::Right,
        CameraOrientation::Top,
        CameraOrientation::Bottom,
        CameraOrientation::Isometric,
    };
    for (const auto orientation : orientations) {
        state.setOrientation(orientation);
        expect(state.orientation() == orientation,
               "every required camera orientation must survive state round trip");
    }
}

}  // namespace

int main() {
    defaultContract();
    allLayerModes();
    coordinateAndSelectionContracts();
    cameraStateContract();

    if (failures != 0) {
        std::cerr << failures << " viewer assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All headless viewer state tests passed\n";
    return EXIT_SUCCESS;
}
