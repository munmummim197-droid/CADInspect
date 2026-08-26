#include <stepcompare/assembly/assembly_index.hpp>
#include <stepcompare/assembly/component_matching.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

stepcompare::domain::GeometryStatistics statistics(double volume = 100.0,
                                                   double area = 60.0) {
    stepcompare::domain::GeometryStatistics value{};
    value.boundingBox = {{-1.0, -2.0, -3.0}, {1.0, 2.0, 3.0}};
    value.volumeMm3 = volume;
    value.surfaceAreaMm2 = area;
    value.centerOfMassMm = {};
    value.topology = {.solids = 1,
                      .shells = 1,
                      .faces = 6,
                      .edges = 12,
                      .vertices = 8};
    value.principalInertia.moments = {10.0, 20.0, 30.0};
    value.principalInertia.axes = {{{1.0, 0.0, 0.0},
                                    {0.0, 1.0, 0.0},
                                    {0.0, 0.0, 1.0}}};
    value.closedSolidEvidence = true;
    return value;
}

stepcompare::import::RigidTransformMm transform(double x = 0.0,
                                                double y = 0.0,
                                                double z = 0.0,
                                                double angleDegrees = 0.0) {
    stepcompare::import::RigidTransformMm value{};
    const auto radians = angleDegrees * std::numbers::pi / 180.0;
    const auto cosine = std::cos(radians);
    const auto sine = std::sin(radians);
    value.matrix = {cosine, -sine, 0.0, x,
                    sine, cosine,  0.0, y,
                    0.0,  0.0,    1.0, z,
                    0.0,  0.0,    0.0, 1.0};
    return value;
}

stepcompare::import::PartPrototype prototype(std::string id,
                                             double volume = 100.0) {
    return {.id = std::move(id),
            .nameUtf8 = "Part",
            .statistics = statistics(volume),
            .geometry = {}};
}

stepcompare::import::AssemblyNode occurrence(
    std::string id,
    std::string prototypeId,
    stepcompare::import::RigidTransformMm placement = {},
    std::string name = {}) {
    return {.id = std::move(id),
            .parentId = std::nullopt,
            .childIds = {},
            .prototypeId = std::move(prototypeId),
            .nameUtf8 = std::move(name),
            .localTransform = placement,
            .isAssembly = false,
            .isInstance = true};
}

stepcompare::import::ImportedModel model(
    std::vector<stepcompare::import::PartPrototype> prototypes,
    std::vector<stepcompare::import::AssemblyNode> nodes) {
    stepcompare::import::ImportedModel value{};
    value.lengthUnit.status =
        stepcompare::import::UnitNormalizationStatus::NormalizedToMillimetres;
    for (const auto& node : nodes) {
        value.rootNodeIds.push_back(node.id);
    }
    value.prototypes = std::move(prototypes);
    value.nodes = std::move(nodes);
    return value;
}

stepcompare::assembly::AssemblyIndex requireIndex(
    const stepcompare::import::ImportedModel& imported) {
    auto result = stepcompare::assembly::buildAssemblyIndex(imported);
    expect(static_cast<bool>(result), "fixture assembly index must build");
    return result ? std::move(*result.index)
                  : stepcompare::assembly::AssemblyIndex{};
}

void indexTests() {
    using namespace stepcompare;
    import::ImportedModel imported{};
    imported.prototypes.push_back(prototype("bolt"));
    imported.rootNodeIds = {"root"};
    imported.nodes.push_back({.id = "root",
                              .parentId = std::nullopt,
                              .childIds = {"bolt-1", "bolt-2"},
                              .prototypeId = std::nullopt,
                              .nameUtf8 = "Assembly",
                              .localTransform = transform(10.0),
                              .isAssembly = true,
                              .isInstance = false});
    auto first = occurrence("bolt-1", "bolt", transform(2.0));
    first.parentId = "root";
    auto second = occurrence("bolt-2", "bolt", transform(4.0));
    second.parentId = "root";
    imported.nodes.push_back(std::move(first));
    imported.nodes.push_back(std::move(second));

    const auto result = assembly::buildAssemblyIndex(imported);
    expect(static_cast<bool>(result), "valid assembly must build an index");
    if (result) {
        expect(result.index->prototypes.size() == 1,
               "one shared prototype must remain one indexed prototype");
        expect(result.index->occurrences.size() == 2,
               "both prototype occurrences must be indexed");
        expect(result.index->prototypes[0].occurrenceIndices.size() == 2,
               "prototype must reference both occurrences without geometry copies");
        const auto* indexed = result.index->findOccurrence("bolt-1");
        expect(indexed != nullptr &&
                   std::abs(indexed->worldTransform.matrix[3] - 12.0) < 1.0e-12,
               "world transform must compose parent and local placement");
    }

    auto malformed = model({prototype("known")},
                           {occurrence("part", "missing")});
    const auto rejected = assembly::buildAssemblyIndex(malformed);
    expect(!rejected, "missing prototype references must fail closed");
    expect(!rejected.diagnostics.empty() &&
               rejected.diagnostics.front().code ==
                   assembly::IndexDiagnosticCode::MissingPrototype,
           "index rejection must explain the missing prototype");

    const auto empty = assembly::buildAssemblyIndex({});
    expect(!empty, "an imported model without roots must fail closed");
}

void exactPlacementTests() {
    using namespace stepcompare::assembly;
    const auto a = requireIndex(model(
        {prototype("part")},
        {occurrence("moved", "part"), occurrence("rotated", "part")}));
    const auto b = requireIndex(model(
        {prototype("part")},
        {occurrence("moved", "part", transform(5.0)),
         occurrence("rotated", "part", transform(0.0, 0.0, 0.0, 0.5))}));
    MatchingOptions options{};
    options.stableNodeIdsTrusted = true;
    options.stablePrototypeIdsTrusted = true;
    const auto result = matchComponents(a, b, options);
    expect(result.rows.size() == 2, "two exact occurrences must produce two rows");
    const auto* moved = result.rows[0].nodeIdA == "moved" ? &result.rows[0]
                                                           : &result.rows[1];
    const auto* rotated = result.rows[0].nodeIdA == "rotated" ? &result.rows[0]
                                                               : &result.rows[1];
    expect(moved->matchStatus == MatchStatus::MatchExact &&
               moved->resultStatus == ComponentResultStatus::Moved &&
               std::abs(moved->translationBMinusAMm.x - 5.0) < 1.0e-12,
           "trusted exact instance must report B-minus-A movement");
    expect(rotated->resultStatus == ComponentResultStatus::Rotated &&
               std::abs(rotated->rotationAngleDegrees - 0.5) < 1.0e-9,
           "trusted exact instance must report rotation");
}

void changedMissingAndNewTests() {
    using namespace stepcompare::assembly;
    const auto a = requireIndex(model(
        {prototype("common"), prototype("old", 50.0)},
        {occurrence("changed", "common"), occurrence("missing", "old")}));
    const auto b = requireIndex(model(
        {prototype("common", 110.0), prototype("new", 500.0)},
        {occurrence("changed", "common"), occurrence("new", "new")}));
    MatchingOptions options{};
    options.stableNodeIdsTrusted = true;
    options.stablePrototypeIdsTrusted = true;
    const auto result = matchComponents(a, b, options);
    bool changed = false;
    bool missing = false;
    bool added = false;
    for (const auto& row : result.rows) {
        changed |= row.resultStatus == ComponentResultStatus::GeometryChanged;
        missing |= row.resultStatus == ComponentResultStatus::Missing;
        added |= row.resultStatus == ComponentResultStatus::New;
    }
    expect(changed, "different invariant statistics must prove geometry changed");
    expect(missing, "unmatched A occurrence must be MISSING");
    expect(added, "unmatched B occurrence must be NEW");
    expect(result.hasDifferences, "changed/missing/new must mark differences");
}

void prototypeDeepDeduplicationTests() {
    using namespace stepcompare::assembly;
    const auto a = requireIndex(model(
        {prototype("A-prototype")},
        {occurrence("A-1", "A-prototype", transform(0.0), "one"),
         occurrence("A-2", "A-prototype", transform(20.0), "two")}));
    const auto b = requireIndex(model(
        {prototype("B-prototype")},
        {occurrence("B-1", "B-prototype", transform(0.0), "one"),
         occurrence("B-2", "B-prototype", transform(20.0), "two")}));
    int verifierCalls = 0;
    MatchingOptions options{};
    options.deepVerifier = [&verifierCalls](const IndexedPrototype&,
                                             const IndexedPrototype&) {
        ++verifierCalls;
        return DeepVerification{GeometryEvidence::SameProven, 1.0};
    };
    const auto result = matchComponents(a, b, options);
    expect(verifierCalls == 1 && result.deepPrototypePairChecks == 1,
           "repeated instances must deep-check a prototype pair only once");
    expect(result.rows.size() == 2,
           "both repeated occurrences must still receive match rows");
    expect(result.rows[0].geometryEvidence == GeometryEvidence::SameProven &&
               result.rows[1].geometryEvidence == GeometryEvidence::SameProven,
           "cached prototype proof must serve every occurrence");
}

void failClosedCandidateTests() {
    using namespace stepcompare::assembly;
    const auto oneA = requireIndex(model(
        {prototype("A")}, {occurrence("A-1", "A")}));
    const auto oneB = requireIndex(model(
        {prototype("B")}, {occurrence("B-1", "B")}));
    const auto probable = matchComponents(oneA, oneB);
    expect(probable.rows.size() == 1 &&
               probable.rows[0].matchStatus == MatchStatus::MatchProbable &&
               probable.rows[0].resultStatus == ComponentResultStatus::Check,
           "compatible invariants without deep proof must remain CHECK");

    const auto ambiguousB = requireIndex(model(
        {prototype("B")},
        {occurrence("B-left", "B", transform(-1.0)),
         occurrence("B-right", "B", transform(1.0))}));
    const auto ambiguous = matchComponents(oneA, ambiguousB);
    expect(!ambiguous.rows.empty() &&
               ambiguous.rows[0].matchStatus == MatchStatus::Ambiguous &&
               ambiguous.rows[0].resultStatus == ComponentResultStatus::Ambiguous,
           "equally plausible candidates must remain AMBIGUOUS");
    expect(!ambiguous.completeWithoutAmbiguity,
           "ambiguous matching must make the result incomplete");
}

}  // namespace

int main() {
    indexTests();
    exactPlacementTests();
    changedMissingAndNewTests();
    prototypeDeepDeduplicationTests();
    failClosedCandidateTests();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All assembly tests passed\n";
    return EXIT_SUCCESS;
}
