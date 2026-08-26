#pragma once

#include <stepcompare/assembly/assembly_index.hpp>
#include <stepcompare/domain/types.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace stepcompare::assembly {

enum class MatchStatus {
    MatchExact,
    MatchGeometry,
    MatchProbable,
    Ambiguous,
    NotMatched,
};

enum class GeometryEvidence {
    SameProven,
    DifferentProven,
    Inconclusive,
};

enum class ComponentResultStatus {
    Same,
    Moved,
    Rotated,
    MovedAndRotated,
    GeometryChanged,
    Missing,
    New,
    Ambiguous,
    Check,
};

struct DeepVerification final {
    GeometryEvidence evidence{GeometryEvidence::Inconclusive};
    double confidence{};
};

using DeepGeometryVerifier = std::function<DeepVerification(
    const IndexedPrototype& a,
    const IndexedPrototype& b)>;

struct MatchingOptions final {
    domain::ToleranceSet tolerances{};
    // These flags are explicit trust boundaries. Exporter-local IDs and names
    // are not considered stable unless the caller opts in.
    bool stableNodeIdsTrusted{};
    bool stablePrototypeIdsTrusted{};
    bool namesTrusted{};
    DeepGeometryVerifier deepVerifier{};
};

struct ComponentMatchRow final {
    std::optional<std::string> nodeIdA{};
    std::optional<std::string> nodeIdB{};
    std::vector<std::string> candidateNodeIdsB{};
    MatchStatus matchStatus{MatchStatus::NotMatched};
    ComponentResultStatus resultStatus{ComponentResultStatus::Check};
    GeometryEvidence geometryEvidence{GeometryEvidence::Inconclusive};
    domain::Vec3Mm translationBMinusAMm{};
    double rotationAngleDegrees{};
    double confidence{};
};

struct AssemblyMatchResult final {
    std::vector<ComponentMatchRow> rows{};
    std::size_t deepPrototypePairChecks{};
    bool completeWithoutAmbiguity{};
    bool hasDifferences{};
};

// Performs correspondence matching in tiers: explicitly trusted identity,
// invariant-property screening, optional deep prototype verification, then
// spatial/context disambiguation. Probable evidence never produces Same.
[[nodiscard]] AssemblyMatchResult matchComponents(
    const AssemblyIndex& a,
    const AssemblyIndex& b,
    const MatchingOptions& options = {});

}  // namespace stepcompare::assembly
