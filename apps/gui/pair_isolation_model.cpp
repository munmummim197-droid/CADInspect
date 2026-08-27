#include "pair_isolation_model.hpp"

#include <cmath>

namespace stepcompare::gui {
namespace {

constexpr std::string_view kPrefixA{"preview/A/"};
constexpr std::string_view kPrefixB{"preview/B/"};
constexpr double kMinimumAutoPairConfidence = 0.80;

std::string_view parsedNodeId(const std::string_view stableId) noexcept {
    if (stableId.starts_with(kPrefixA)) {
        return stableId.substr(kPrefixA.size());
    }
    if (stableId.starts_with(kPrefixB)) {
        return stableId.substr(kPrefixB.size());
    }
    return {};
}

bool autoPairEvidence(const stepcompare::reporting::ComponentRow& row) noexcept {
    return !row.idA.empty() && !row.idB.empty() &&
           (row.matchStatus == "MATCH_EXACT" ||
            row.matchStatus == "MATCH_GEOMETRY") &&
           std::isfinite(row.confidence) &&
           row.confidence >= kMinimumAutoPairConfidence;
}

}  // namespace

bool isOccurrenceStableId(const std::string_view stableId) noexcept {
    return !parsedNodeId(stableId).empty();
}

bool isSideAStableId(const std::string_view stableId) noexcept {
    return stableId.starts_with(kPrefixA) && isOccurrenceStableId(stableId);
}

std::string_view occurrenceNodeId(const std::string_view stableId) noexcept {
    return parsedNodeId(stableId);
}

PairResolution resolveCanonicalPair(
    const stepcompare::reporting::Report& report,
    const std::string_view selectedStableId) noexcept {
    if (!isOccurrenceStableId(selectedStableId)) {
        return {.status = PairResolutionStatus::NotAnOccurrence};
    }

    const bool selectedA = isSideAStableId(selectedStableId);
    const std::string_view selectedNode = parsedNodeId(selectedStableId);
    const stepcompare::reporting::ComponentRow* match = nullptr;
    for (const auto& row : report.components) {
        const bool matches = selectedA ? row.idA == selectedNode
                                       : row.idB == selectedNode;
        if (!matches) {
            continue;
        }
        if (match != nullptr) {
            return {.status = PairResolutionStatus::MatchAmbiguous};
        }
        match = &row;
    }
    if (match == nullptr || !autoPairEvidence(*match)) {
        return {.status = PairResolutionStatus::MatchAmbiguous};
    }
    return {
        .status = PairResolutionStatus::Resolved,
        .stableIdA = std::string(kPrefixA) + match->idA,
        .stableIdB = std::string(kPrefixB) + match->idB,
        .confidence = match->confidence,
    };
}

}  // namespace stepcompare::gui
