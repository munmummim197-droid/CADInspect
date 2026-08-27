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
    expect(state.presentationMode() == PresentationMode::ShadedWithEdges,
           "default presentation must expose feature edges");
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

void presentationModesContract() {
    using namespace stepcompare::viewer;
    ViewerStateModel state;
    constexpr PresentationMode modes[]{
        PresentationMode::Shaded,
        PresentationMode::ShadedWithEdges,
        PresentationMode::Wireframe,
        PresentationMode::TransparentXRay,
        PresentationMode::Section,
    };
    for (const auto mode : modes) {
        state.setPresentationMode(mode);
        expect(state.presentationMode() == mode,
               "every presentation mode must survive state round trip");
        expect(toString(mode) != "",
               "every presentation mode must have an explicit label");
    }
}

void sectionSettingsContract() {
    using namespace stepcompare::viewer;
    ViewerStateModel state;
    expect(state.sectionSettings() == SectionSettings{},
           "section controls must start at camera, centered, A+B");

    constexpr SectionDirection directions[]{
        SectionDirection::XY,
        SectionDirection::YZ,
        SectionDirection::ZX,
        SectionDirection::Front,
        SectionDirection::Top,
        SectionDirection::Right,
        SectionDirection::Camera,
    };
    for (const auto direction : directions) {
        SectionSettings settings;
        settings.direction = direction;
        settings.target = SectionTarget::B;
        settings.normalizedOffset = 0.35;
        settings.flipped = true;
        state.setSectionSettings(settings);
        expect(state.sectionSettings().direction == direction &&
                   state.sectionSettings().target == SectionTarget::B &&
                   state.sectionSettings().flipped,
               "every section direction must survive state round trip");
        expect(toString(direction) != "",
               "every section direction must have a stable label");
    }
    SectionSettings clamped;
    clamped.normalizedOffset = 5.0;
    state.setSectionSettings(clamped);
    expect(state.sectionSettings().normalizedOffset == 1.0,
           "section offset must remain bounded by the model extent");
    state.resetSectionSettings();
    expect(state.sectionSettings() == SectionSettings{},
           "section reset must restore the practical default");
    expect(toString(SectionTarget::A) == "A" &&
               toString(SectionTarget::B) == "B" &&
               toString(SectionTarget::Both) == "A+B",
           "section target labels must be explicit");
}

}  // namespace

int main() {
    defaultContract();
    allLayerModes();
    coordinateAndSelectionContracts();
    cameraStateContract();
    presentationModesContract();
    sectionSettingsContract();

    if (failures != 0) {
        std::cerr << failures << " viewer assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All headless viewer state tests passed\n";
    return EXIT_SUCCESS;
}
