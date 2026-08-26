#include <stepcompare/assembly/assembly_index.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace stepcompare::assembly {
namespace {

using PrototypeMap = std::unordered_map<std::string, std::size_t>;
using NodeMap = std::unordered_map<std::string, std::size_t>;

void diagnose(std::vector<IndexDiagnostic>& diagnostics,
              IndexDiagnosticCode code, std::string subject,
              std::string message) {
    diagnostics.push_back(
        {code, std::move(subject), std::move(message)});
}

[[nodiscard]] import::RigidTransformMm multiply(
    const import::RigidTransformMm& left,
    const import::RigidTransformMm& right) noexcept {
    import::RigidTransformMm result{};
    result.matrix.fill(0.0);
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            for (std::size_t inner = 0; inner < 4; ++inner) {
                result.matrix[row * 4 + column] +=
                    left.matrix[row * 4 + inner] *
                    right.matrix[inner * 4 + column];
            }
        }
    }
    return result;
}

[[nodiscard]] bool isValidRigidTransform(
    const import::RigidTransformMm& transform) noexcept {
    if (!std::all_of(transform.matrix.begin(), transform.matrix.end(),
                     [](double value) { return std::isfinite(value); })) {
        return false;
    }
    constexpr double tolerance = 1.0e-9;
    if (std::abs(transform.matrix[12]) > tolerance ||
        std::abs(transform.matrix[13]) > tolerance ||
        std::abs(transform.matrix[14]) > tolerance ||
        std::abs(transform.matrix[15] - 1.0) > tolerance) {
        return false;
    }
    for (std::size_t firstColumn = 0; firstColumn < 3; ++firstColumn) {
        for (std::size_t secondColumn = 0; secondColumn < 3; ++secondColumn) {
            double dot = 0.0;
            for (std::size_t row = 0; row < 3; ++row) {
                dot += transform.matrix[row * 4 + firstColumn] *
                       transform.matrix[row * 4 + secondColumn];
            }
            const auto expected = firstColumn == secondColumn ? 1.0 : 0.0;
            if (std::abs(dot - expected) > tolerance) {
                return false;
            }
        }
    }
    const auto& m = transform.matrix;
    const double determinant =
        m[0] * (m[5] * m[10] - m[6] * m[9]) -
        m[1] * (m[4] * m[10] - m[6] * m[8]) +
        m[2] * (m[4] * m[9] - m[5] * m[8]);
    return std::abs(determinant - 1.0) <= tolerance;
}

[[nodiscard]] bool contains(const std::vector<std::string>& values,
                            std::string_view expected) {
    return std::find(values.begin(), values.end(), expected) != values.end();
}

}  // namespace

const IndexedPrototype* AssemblyIndex::findPrototype(
    std::string_view id) const noexcept {
    const auto found = std::find_if(
        prototypes.begin(), prototypes.end(),
        [id](const IndexedPrototype& value) { return value.id == id; });
    return found == prototypes.end() ? nullptr : &*found;
}

const IndexedOccurrence* AssemblyIndex::findOccurrence(
    std::string_view nodeId) const noexcept {
    const auto found = std::find_if(
        occurrences.begin(), occurrences.end(),
        [nodeId](const IndexedOccurrence& value) {
            return value.nodeId == nodeId;
        });
    return found == occurrences.end() ? nullptr : &*found;
}

AssemblyIndexResult buildAssemblyIndex(const import::ImportedModel& model) {
    AssemblyIndexResult result{};
    PrototypeMap prototypeIndices;
    prototypeIndices.reserve(model.prototypes.size());
    for (std::size_t index = 0; index < model.prototypes.size(); ++index) {
        const auto& prototype = model.prototypes[index];
        if (prototype.id.empty() ||
            !prototypeIndices.emplace(prototype.id, index).second) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::DuplicatePrototypeId,
                     prototype.id,
                     "Prototype IDs must be non-empty and unique");
        }
    }

    NodeMap nodeIndices;
    nodeIndices.reserve(model.nodes.size());
    for (std::size_t index = 0; index < model.nodes.size(); ++index) {
        const auto& node = model.nodes[index];
        if (node.id.empty() || !nodeIndices.emplace(node.id, index).second) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::DuplicateNodeId,
                     node.id,
                     "Assembly node IDs must be non-empty and unique");
        }
    }

    std::unordered_set<std::string> rootIds;
    rootIds.reserve(model.rootNodeIds.size());
    if (model.rootNodeIds.empty()) {
        diagnose(result.diagnostics,
                 IndexDiagnosticCode::MissingRootNode,
                 {},
                 "Imported assembly must declare at least one root node");
    }
    for (const auto& rootId : model.rootNodeIds) {
        if (!rootIds.insert(rootId).second) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::DuplicateRootId,
                     rootId,
                     "Root node IDs must be unique");
            continue;
        }
        const auto found = nodeIndices.find(rootId);
        if (found == nodeIndices.end()) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::MissingRootNode,
                     rootId,
                     "Root ID does not reference an assembly node");
            continue;
        }
        if (model.nodes[found->second].parentId) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::InvalidRootParent,
                     rootId,
                     "Root assembly nodes cannot have a parent");
        }
    }

    for (const auto& node : model.nodes) {
        if (!isValidRigidTransform(node.localTransform)) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::InvalidTransform,
                     node.id,
                     "Assembly placement is not a finite rigid transform");
        }
        if (node.isAssembly && node.prototypeId) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::AssemblyReferencesPrototype,
                     node.id,
                     "Assembly nodes cannot reference a part prototype");
        } else if (!node.isAssembly && !node.prototypeId) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::PartMissingPrototype,
                     node.id,
                     "Part occurrence does not reference a prototype");
        } else if (!node.isAssembly && !node.childIds.empty()) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::PartHasChildren,
                     node.id,
                     "Part occurrences cannot contain assembly children");
        } else if (node.prototypeId &&
                   !prototypeIndices.contains(*node.prototypeId)) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::MissingPrototype,
                     *node.prototypeId,
                     "Part occurrence references an unknown prototype");
        }

        if (node.parentId) {
            const auto parent = nodeIndices.find(*node.parentId);
            if (parent == nodeIndices.end()) {
                diagnose(result.diagnostics,
                         IndexDiagnosticCode::MissingParentNode,
                         node.id,
                         "Assembly node references an unknown parent");
            } else if (!contains(model.nodes[parent->second].childIds, node.id)) {
                diagnose(result.diagnostics,
                         IndexDiagnosticCode::InconsistentParentChildLink,
                         node.id,
                         "Parent does not list this node as a child");
            }
        } else if (!rootIds.contains(node.id)) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::UnreachableNode,
                     node.id,
                     "Parentless node is not listed as a root");
        }

        std::unordered_set<std::string> uniqueChildIds;
        uniqueChildIds.reserve(node.childIds.size());
        for (const auto& childId : node.childIds) {
            if (!uniqueChildIds.insert(childId).second) {
                diagnose(result.diagnostics,
                         IndexDiagnosticCode::DuplicateChildLink,
                         childId,
                         "Assembly node lists the same child more than once");
                continue;
            }
            const auto child = nodeIndices.find(childId);
            if (child == nodeIndices.end()) {
                diagnose(result.diagnostics,
                         IndexDiagnosticCode::MissingChildNode,
                         childId,
                         "Assembly node references an unknown child");
            } else if (!model.nodes[child->second].parentId ||
                       *model.nodes[child->second].parentId != node.id) {
                diagnose(result.diagnostics,
                         IndexDiagnosticCode::InconsistentParentChildLink,
                         childId,
                         "Child parent reference does not match its container");
            }
        }
    }

    std::vector<unsigned char> visitState(model.nodes.size(), 0U);
    std::unordered_set<std::string> reachable;
    reachable.reserve(model.nodes.size());
    std::function<void(std::size_t)> visit = [&](std::size_t nodeIndex) {
        if (visitState[nodeIndex] == 1U) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::CycleDetected,
                     model.nodes[nodeIndex].id,
                     "Assembly hierarchy contains a cycle");
            return;
        }
        if (visitState[nodeIndex] == 2U) {
            return;
        }
        visitState[nodeIndex] = 1U;
        reachable.insert(model.nodes[nodeIndex].id);
        for (const auto& childId : model.nodes[nodeIndex].childIds) {
            const auto child = nodeIndices.find(childId);
            if (child != nodeIndices.end()) {
                visit(child->second);
            }
        }
        visitState[nodeIndex] = 2U;
    };
    for (const auto& rootId : model.rootNodeIds) {
        const auto root = nodeIndices.find(rootId);
        if (root != nodeIndices.end()) {
            visit(root->second);
        }
    }
    for (const auto& node : model.nodes) {
        if (!reachable.contains(node.id)) {
            diagnose(result.diagnostics,
                     IndexDiagnosticCode::UnreachableNode,
                     node.id,
                     "Assembly node cannot be reached from a declared root");
        }
    }

    if (!result.diagnostics.empty()) {
        return result;
    }

    AssemblyIndex index;
    index.prototypes.reserve(model.prototypes.size());
    std::unordered_map<std::string, std::size_t> indexedPrototypeIndices;
    indexedPrototypeIndices.reserve(model.prototypes.size());
    for (const auto& prototype : model.prototypes) {
        indexedPrototypeIndices.emplace(prototype.id, index.prototypes.size());
        index.prototypes.push_back({.id = prototype.id,
                                    .nameUtf8 = prototype.nameUtf8,
                                    .statistics = prototype.statistics,
                                    .geometry = prototype.geometry,
                                    .occurrenceIndices = {}});
    }

    std::function<void(std::size_t, const import::RigidTransformMm&)> build =
        [&](std::size_t nodeIndex,
            const import::RigidTransformMm& parentWorld) {
            const auto& node = model.nodes[nodeIndex];
            const auto world = multiply(parentWorld, node.localTransform);
            if (node.prototypeId) {
                const auto prototypeIndex =
                    indexedPrototypeIndices.at(*node.prototypeId);
                const auto occurrenceIndex = index.occurrences.size();
                index.occurrences.push_back({
                    .nodeId = node.id,
                    .parentNodeId = node.parentId,
                    .nameUtf8 = node.nameUtf8,
                    .prototypeIndex = prototypeIndex,
                    .localTransform = node.localTransform,
                    .worldTransform = world,
                    .importedAsInstance = node.isInstance,
                });
                index.prototypes[prototypeIndex].occurrenceIndices.push_back(
                    occurrenceIndex);
            }
            for (const auto& childId : node.childIds) {
                build(nodeIndices.at(childId), world);
            }
        };
    const import::RigidTransformMm identity{};
    for (const auto& rootId : model.rootNodeIds) {
        build(nodeIndices.at(rootId), identity);
    }

    result.index = std::move(index);
    return result;
}

}  // namespace stepcompare::assembly
