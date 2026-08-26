#pragma once

#include <stepcompare/import/imported_model.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace stepcompare::assembly {

enum class IndexDiagnosticCode {
    DuplicatePrototypeId,
    DuplicateNodeId,
    DuplicateRootId,
    DuplicateChildLink,
    MissingRootNode,
    InvalidRootParent,
    MissingParentNode,
    MissingChildNode,
    InconsistentParentChildLink,
    MissingPrototype,
    AssemblyReferencesPrototype,
    PartMissingPrototype,
    PartHasChildren,
    InvalidTransform,
    CycleDetected,
    UnreachableNode,
};

struct IndexDiagnostic final {
    IndexDiagnosticCode code{};
    std::string subjectId{};
    std::string message{};
};

struct IndexedPrototype final {
    std::string id{};
    std::string nameUtf8{};
    domain::GeometryStatistics statistics{};
    import::GeometryPayloadPtr geometry{};
    std::vector<std::size_t> occurrenceIndices{};
};

struct IndexedOccurrence final {
    std::string nodeId{};
    std::optional<std::string> parentNodeId{};
    std::string nameUtf8{};
    std::size_t prototypeIndex{};
    import::RigidTransformMm localTransform{};
    import::RigidTransformMm worldTransform{};
    bool importedAsInstance{};
};

struct AssemblyIndex final {
    std::vector<IndexedPrototype> prototypes{};
    std::vector<IndexedOccurrence> occurrences{};

    [[nodiscard]] const IndexedPrototype* findPrototype(
        std::string_view id) const noexcept;
    [[nodiscard]] const IndexedOccurrence* findOccurrence(
        std::string_view nodeId) const noexcept;
};

struct AssemblyIndexResult final {
    std::optional<AssemblyIndex> index{};
    std::vector<IndexDiagnostic> diagnostics{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return index.has_value();
    }
};

// Builds a validated, immutable-by-convention index. Geometry payloads remain
// shared per prototype; occurrences never duplicate native geometry handles.
[[nodiscard]] AssemblyIndexResult buildAssemblyIndex(
    const import::ImportedModel& model);

}  // namespace stepcompare::assembly
