#include "step_preview_model.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace stepcompare::gui {
namespace {

using Matrix = std::array<double, 16>;

std::string sidePrefix(const stepcompare::viewer::ModelSide side) {
    return side == stepcompare::viewer::ModelSide::A ? "preview/A" : "preview/B";
}

std::string fileNameUtf8(const std::u8string& sourcePathUtf8) {
    const auto name = std::filesystem::path(sourcePathUtf8).filename().u8string();
    return {reinterpret_cast<const char*>(name.data()), name.size()};
}

Matrix multiply(const Matrix& left, const Matrix& right) noexcept {
    Matrix result{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            for (std::size_t inner = 0; inner < 4; ++inner) {
                result[row * 4 + column] +=
                    left[row * 4 + inner] * right[inner * 4 + column];
            }
        }
    }
    return result;
}

}  // namespace

PreviewScenePlan buildPreviewScenePlan(
    const stepcompare::import::ImportedModel& model,
    const stepcompare::viewer::ModelSide side) {
    using stepcompare::viewer::ComponentChangeKind;
    using stepcompare::viewer::ResultRowSnapshot;
    using stepcompare::viewer::StableSelectionId;

    const std::string prefix = sidePrefix(side);
    PreviewScenePlan plan;
    plan.rows.reserve(model.nodes.size() + 1U);
    plan.occurrences.reserve(model.nodes.size());
    plan.rows.push_back({StableSelectionId{prefix},
                         fileNameUtf8(model.sourcePathUtf8),
                         ComponentChangeKind::Unchanged});

    std::unordered_map<std::string, Matrix> worldTransforms;
    worldTransforms.reserve(model.nodes.size());
    for (const auto& node : model.nodes) {
        Matrix world = node.localTransform.matrix;
        if (node.parentId) {
            const auto parent = worldTransforms.find(*node.parentId);
            if (parent == worldTransforms.end()) {
                throw std::invalid_argument(
                    "Preview assembly parent is not ordered before child");
            }
            world = multiply(parent->second, node.localTransform.matrix);
        }
        if (!worldTransforms.emplace(node.id, world).second) {
            throw std::invalid_argument("Duplicate preview assembly node ID");
        }

        const std::string stableValue = prefix + "/" + node.id;
        const std::string parentValue = node.parentId ? prefix + "/" + *node.parentId
                                                      : prefix;
        plan.rows.push_back({StableSelectionId{stableValue},
                             node.nameUtf8.empty() ? node.id : node.nameUtf8,
                             ComponentChangeKind::Unchanged,
                             StableSelectionId{parentValue}});
        if (node.prototypeId) {
            plan.occurrences.push_back({StableSelectionId{stableValue},
                                        *node.prototypeId,
                                        world});
        }
    }
    return plan;
}

}  // namespace stepcompare::gui
