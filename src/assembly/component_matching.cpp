#include <stepcompare/assembly/component_matching.hpp>

#include <stepcompare/domain/fast_check.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace stepcompare::assembly {
namespace {

struct PrototypePair final {
    std::size_t a{};
    std::size_t b{};

    [[nodiscard]] friend bool operator==(const PrototypePair&,
                                         const PrototypePair&) = default;
};

struct PrototypePairHash final {
    [[nodiscard]] std::size_t operator()(const PrototypePair& value) const
        noexcept {
        const auto seed = std::hash<std::size_t>{}(value.a);
        return seed ^ (std::hash<std::size_t>{}(value.b) +
                       0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
    }
};

struct PairEvidence final {
    GeometryEvidence evidence{GeometryEvidence::Inconclusive};
    double confidence{};
};

class EvidenceCache final {
public:
    EvidenceCache(const AssemblyIndex& a, const AssemblyIndex& b,
                  const MatchingOptions& options)
        : a_(a), b_(b), options_(options) {}

    [[nodiscard]] PairEvidence get(std::size_t prototypeA,
                                   std::size_t prototypeB) {
        const PrototypePair key{prototypeA, prototypeB};
        if (const auto found = cache_.find(key); found != cache_.end()) {
            return found->second;
        }

        const auto& a = a_.prototypes[prototypeA];
        const auto& b = b_.prototypes[prototypeB];
        const auto fast = domain::compareFastInvariants(
            a.statistics, b.statistics, options_.tolerances);
        PairEvidence result{};
        if (fast.status == domain::FastScreenStatus::Different) {
            result = {GeometryEvidence::DifferentProven, 1.0};
        } else if (fast.status == domain::FastScreenStatus::CompatibleCandidate &&
                   options_.stablePrototypeIdsTrusted && a.id == b.id) {
            result = {GeometryEvidence::SameProven, 1.0};
        } else if (options_.deepVerifier) {
            ++deepChecks_;
            try {
                const auto verified = options_.deepVerifier(a, b);
                result.evidence = verified.evidence;
                result.confidence =
                    std::isfinite(verified.confidence)
                        ? std::clamp(verified.confidence, 0.0, 1.0)
                        : 0.0;
            } catch (...) {
                // Adapter failures cannot become a false match.
                result = {GeometryEvidence::Inconclusive, 0.0};
            }
        }
        cache_.emplace(key, result);
        return result;
    }

    [[nodiscard]] std::size_t deepChecks() const noexcept {
        return deepChecks_;
    }

private:
    const AssemblyIndex& a_;
    const AssemblyIndex& b_;
    const MatchingOptions& options_;
    std::unordered_map<PrototypePair, PairEvidence, PrototypePairHash> cache_;
    std::size_t deepChecks_{};
};

[[nodiscard]] domain::Vec3Mm translation(
    const import::RigidTransformMm& transform) noexcept {
    return {transform.matrix[3], transform.matrix[7], transform.matrix[11]};
}

[[nodiscard]] domain::Vec3Mm deltaBMinusA(
    const IndexedOccurrence& a,
    const IndexedOccurrence& b) noexcept {
    const auto positionA = translation(a.worldTransform);
    const auto positionB = translation(b.worldTransform);
    return {positionB.x - positionA.x,
            positionB.y - positionA.y,
            positionB.z - positionA.z};
}

[[nodiscard]] double squaredLength(const domain::Vec3Mm& value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] double rotationAngleDegrees(
    const import::RigidTransformMm& a,
    const import::RigidTransformMm& b) noexcept {
    double relativeTrace = 0.0;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            relativeTrace +=
                a.matrix[row * 4 + column] * b.matrix[row * 4 + column];
        }
    }
    const auto cosine = std::clamp((relativeTrace - 1.0) * 0.5, -1.0, 1.0);
    constexpr double radiansToDegrees =
        180.0 / 3.141592653589793238462643383279502884;
    return std::acos(cosine) * radiansToDegrees;
}

[[nodiscard]] ComponentResultStatus placementStatus(
    const domain::Vec3Mm& delta,
    double angleDegrees,
    const domain::ToleranceSet& tolerances) noexcept {
    const bool moved =
        std::sqrt(squaredLength(delta)) > tolerances.positionMm;
    const bool rotated = angleDegrees > tolerances.angularDegrees;
    if (moved && rotated) {
        return ComponentResultStatus::MovedAndRotated;
    }
    if (moved) {
        return ComponentResultStatus::Moved;
    }
    if (rotated) {
        return ComponentResultStatus::Rotated;
    }
    return ComponentResultStatus::Same;
}

void finalizeDifferenceFlags(AssemblyMatchResult& result) {
    result.completeWithoutAmbiguity = true;
    result.hasDifferences = false;
    for (const auto& row : result.rows) {
        if (row.resultStatus == ComponentResultStatus::Ambiguous ||
            row.resultStatus == ComponentResultStatus::Check) {
            result.completeWithoutAmbiguity = false;
        }
        switch (row.resultStatus) {
        case ComponentResultStatus::Moved:
        case ComponentResultStatus::Rotated:
        case ComponentResultStatus::MovedAndRotated:
        case ComponentResultStatus::GeometryChanged:
        case ComponentResultStatus::Missing:
        case ComponentResultStatus::New:
            result.hasDifferences = true;
            break;
        case ComponentResultStatus::Same:
        case ComponentResultStatus::Ambiguous:
        case ComponentResultStatus::Check:
            break;
        }
    }
}

[[nodiscard]] ComponentMatchRow pairedRow(
    const IndexedOccurrence& occurrenceA,
    const IndexedOccurrence& occurrenceB,
    MatchStatus matchStatus,
    PairEvidence evidence,
    const domain::ToleranceSet& tolerances) {
    ComponentMatchRow row{};
    row.nodeIdA = occurrenceA.nodeId;
    row.nodeIdB = occurrenceB.nodeId;
    row.matchStatus = matchStatus;
    row.geometryEvidence = evidence.evidence;
    row.confidence = evidence.confidence;
    row.translationBMinusAMm = deltaBMinusA(occurrenceA, occurrenceB);
    row.rotationAngleDegrees = rotationAngleDegrees(
        occurrenceA.worldTransform, occurrenceB.worldTransform);
    if (evidence.evidence == GeometryEvidence::DifferentProven) {
        row.resultStatus = ComponentResultStatus::GeometryChanged;
    } else if (evidence.evidence != GeometryEvidence::SameProven ||
               matchStatus == MatchStatus::MatchProbable) {
        row.resultStatus = ComponentResultStatus::Check;
    } else {
        row.resultStatus = placementStatus(row.translationBMinusAMm,
                                           row.rotationAngleDegrees,
                                           tolerances);
    }
    return row;
}

struct Candidate final {
    std::size_t bIndex{};
    PairEvidence evidence{};
    double squaredDistance{};
};

}  // namespace

AssemblyMatchResult matchComponents(const AssemblyIndex& a,
                                    const AssemblyIndex& b,
                                    const MatchingOptions& options) {
    AssemblyMatchResult result{};
    EvidenceCache evidence(a, b, options);
    std::vector<bool> matchedA(a.occurrences.size(), false);
    std::vector<bool> matchedB(b.occurrences.size(), false);
    std::vector<bool> reservedByAmbiguityB(b.occurrences.size(), false);

    const auto matchTrustedPairs = [&](bool byNodeId) {
        for (std::size_t indexA = 0; indexA < a.occurrences.size(); ++indexA) {
            if (matchedA[indexA]) {
                continue;
            }
            const auto& occurrenceA = a.occurrences[indexA];
            std::optional<std::size_t> candidateB;
            bool duplicate = false;
            for (std::size_t indexB = 0; indexB < b.occurrences.size(); ++indexB) {
                if (matchedB[indexB]) {
                    continue;
                }
                const auto& occurrenceB = b.occurrences[indexB];
                // A node path is a correspondence key, not proof that the
                // geometry is the same.  Guard it with the occurrence name
                // when both exporters supplied one so an unrelated component
                // that reused an exporter-local label cannot be paired by
                // structural position alone.  The selected pair still goes
                // through invariant/deep geometry verification below.
                const bool namesCompatible =
                    occurrenceA.nameUtf8.empty() ||
                    occurrenceB.nameUtf8.empty() ||
                    occurrenceA.nameUtf8 == occurrenceB.nameUtf8;
                const bool equal = byNodeId
                                       ? occurrenceA.nodeId == occurrenceB.nodeId &&
                                             namesCompatible
                                       : (!occurrenceA.nameUtf8.empty() &&
                                          occurrenceA.nameUtf8 ==
                                              occurrenceB.nameUtf8);
                if (equal) {
                    if (candidateB) {
                        duplicate = true;
                        break;
                    }
                    candidateB = indexB;
                }
            }
            if (!candidateB || duplicate) {
                continue;
            }
            if (!byNodeId) {
                std::size_t sameNameCountA = 0;
                for (std::size_t otherA = 0; otherA < a.occurrences.size();
                     ++otherA) {
                    if (!matchedA[otherA] &&
                        a.occurrences[otherA].nameUtf8 == occurrenceA.nameUtf8) {
                        ++sameNameCountA;
                    }
                }
                if (sameNameCountA != 1) {
                    continue;
                }
            }
            const auto pairEvidence = evidence.get(
                occurrenceA.prototypeIndex,
                b.occurrences[*candidateB].prototypeIndex);
            result.rows.push_back(pairedRow(occurrenceA,
                                            b.occurrences[*candidateB],
                                            MatchStatus::MatchExact,
                                            pairEvidence,
                                            options.tolerances));
            matchedA[indexA] = true;
            matchedB[*candidateB] = true;
        }
    };

    if (options.stableNodeIdsTrusted) {
        matchTrustedPairs(true);
    }
    if (options.namesTrusted) {
        matchTrustedPairs(false);
    }

    for (std::size_t indexA = 0; indexA < a.occurrences.size(); ++indexA) {
        if (matchedA[indexA]) {
            continue;
        }
        const auto& occurrenceA = a.occurrences[indexA];
        std::vector<Candidate> candidates;
        for (std::size_t indexB = 0; indexB < b.occurrences.size(); ++indexB) {
            if (matchedB[indexB] || reservedByAmbiguityB[indexB]) {
                continue;
            }
            const auto& occurrenceB = b.occurrences[indexB];
            if (options.namesTrusted && !occurrenceA.nameUtf8.empty() &&
                !occurrenceB.nameUtf8.empty() &&
                occurrenceA.nameUtf8 != occurrenceB.nameUtf8) {
                continue;
            }
            const auto pairEvidence = evidence.get(
                occurrenceA.prototypeIndex, occurrenceB.prototypeIndex);
            if (pairEvidence.evidence == GeometryEvidence::DifferentProven) {
                continue;
            }
            candidates.push_back({indexB,
                                  pairEvidence,
                                  squaredLength(deltaBMinusA(occurrenceA,
                                                             occurrenceB))});
        }

        if (candidates.empty()) {
            continue;
        }
        const auto best = std::min_element(
            candidates.begin(), candidates.end(),
            [](const Candidate& left, const Candidate& right) {
                return left.squaredDistance < right.squaredDistance;
            });
        const auto tieThreshold =
            options.tolerances.positionMm * options.tolerances.positionMm;
        std::vector<const Candidate*> tied;
        for (const auto& candidate : candidates) {
            if (std::abs(candidate.squaredDistance - best->squaredDistance) <=
                tieThreshold) {
                tied.push_back(&candidate);
            }
        }
        if (tied.size() != 1) {
            ComponentMatchRow row{};
            row.nodeIdA = occurrenceA.nodeId;
            row.matchStatus = MatchStatus::Ambiguous;
            row.resultStatus = ComponentResultStatus::Ambiguous;
            for (const auto* candidate : tied) {
                row.candidateNodeIdsB.push_back(
                    b.occurrences[candidate->bIndex].nodeId);
                reservedByAmbiguityB[candidate->bIndex] = true;
            }
            result.rows.push_back(std::move(row));
            matchedA[indexA] = true;
            continue;
        }

        const auto& selected = *tied.front();
        const bool correspondenceProven = candidates.size() == 1;
        const auto status = selected.evidence.evidence ==
                                    GeometryEvidence::SameProven &&
                                correspondenceProven
                                ? MatchStatus::MatchGeometry
                                : MatchStatus::MatchProbable;
        result.rows.push_back(pairedRow(occurrenceA,
                                        b.occurrences[selected.bIndex],
                                        status,
                                        selected.evidence,
                                        options.tolerances));
        matchedA[indexA] = true;
        matchedB[selected.bIndex] = true;
    }

    for (std::size_t indexA = 0; indexA < a.occurrences.size(); ++indexA) {
        if (!matchedA[indexA]) {
            ComponentMatchRow row{};
            row.nodeIdA = a.occurrences[indexA].nodeId;
            row.matchStatus = MatchStatus::NotMatched;
            row.resultStatus = ComponentResultStatus::Missing;
            result.rows.push_back(std::move(row));
        }
    }
    for (std::size_t indexB = 0; indexB < b.occurrences.size(); ++indexB) {
        if (!matchedB[indexB] && !reservedByAmbiguityB[indexB]) {
            ComponentMatchRow row{};
            row.nodeIdB = b.occurrences[indexB].nodeId;
            row.matchStatus = MatchStatus::NotMatched;
            row.resultStatus = ComponentResultStatus::New;
            result.rows.push_back(std::move(row));
        }
    }

    result.deepPrototypePairChecks = evidence.deepChecks();
    finalizeDifferenceFlags(result);
    return result;
}

}  // namespace stepcompare::assembly
