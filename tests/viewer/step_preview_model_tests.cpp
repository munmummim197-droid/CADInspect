#include "step_preview_model.hpp"

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

stepcompare::import::RigidTransformMm translated(const double x,
                                                 const double y,
                                                 const double z) {
    stepcompare::import::RigidTransformMm transform;
    transform.matrix[3] = x;
    transform.matrix[7] = y;
    transform.matrix[11] = z;
    return transform;
}

stepcompare::import::ImportedModel model() {
    stepcompare::import::ImportedModel imported;
    imported.sourcePathUtf8 = u8"D:/Dự án/Bản vẽ/Chi tiết 01.step";
    imported.nodes.push_back({.id = "root",
                              .childIds = {"root/part"},
                              .nameUtf8 = "Cụm chính",
                              .localTransform = translated(5.0, 0.0, 0.0),
                              .isAssembly = true});
    imported.nodes.push_back({.id = "root/part",
                              .parentId = "root",
                              .prototypeId = "prototype-1",
                              .nameUtf8 = "Chi tiết",
                              .localTransform = translated(2.0, 3.0, 0.0),
                              .isInstance = true});
    return imported;
}

void unicodeHierarchyAndWorldTransform() {
    using namespace stepcompare;
    const auto plan = gui::buildPreviewScenePlan(model(), viewer::ModelSide::A);
    expect(plan.rows.size() == 3, "preview plan must contain file root and both nodes");
    expect(plan.rows[0].label == "Chi tiết 01.step",
           "Unicode filename must survive preview plan conversion");
    expect(plan.rows[2].stableId.value() == "preview/A/root/part",
           "side-qualified node ID must be stable");
    expect(plan.rows[2].parentStableId &&
               plan.rows[2].parentStableId->value() == "preview/A/root",
           "assembly hierarchy must be preserved");
    expect(plan.occurrences.size() == 1, "only part instances become render occurrences");
    expect(plan.occurrences[0].worldTransform[3] == 7.0 &&
               plan.occurrences[0].worldTransform[7] == 3.0,
           "world transform must compose parent then local placement");

    const auto planB = gui::buildPreviewScenePlan(model(), viewer::ModelSide::B);
    expect(planB.occurrences[0].stableId.value() == "preview/B/root/part",
           "A and B occurrences must never collide in overlay");
}

void invalidAssemblyOrderFailsClosed() {
    using namespace stepcompare;
    auto imported = model();
    std::swap(imported.nodes[0], imported.nodes[1]);
    bool rejected = false;
    try {
        static_cast<void>(gui::buildPreviewScenePlan(imported, viewer::ModelSide::A));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, "child-before-parent scene data must fail closed");
}

}  // namespace

int main() {
    unicodeHierarchyAndWorldTransform();
    invalidAssemblyOrderFailsClosed();
    if (failures != 0) {
        std::cerr << failures << " preview model assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All headless STEP preview model tests passed\n";
    return EXIT_SUCCESS;
}
