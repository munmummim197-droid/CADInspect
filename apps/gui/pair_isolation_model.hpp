#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <stepcompare/reporting/report.hpp>

namespace stepcompare::gui {

enum class PairResolutionStatus {
    Resolved,
    MatchAmbiguous,
    NotAnOccurrence,
};

struct PairResolution final {
    PairResolutionStatus status{PairResolutionStatus::NotAnOccurrence};
    std::string stableIdA{};
    std::string stableIdB{};
    double confidence{};

    [[nodiscard]] bool resolved() const noexcept {
        return status == PairResolutionStatus::Resolved;
    }
};

// Resolves only canonical occurrence pairs. Part names are intentionally not
// accepted as evidence because repeated prototypes can have many instances.
[[nodiscard]] PairResolution resolveCanonicalPair(
    const stepcompare::reporting::Report& report,
    std::string_view selectedStableId) noexcept;

[[nodiscard]] bool isOccurrenceStableId(std::string_view stableId) noexcept;
[[nodiscard]] bool isSideAStableId(std::string_view stableId) noexcept;
[[nodiscard]] std::string_view occurrenceNodeId(
    std::string_view stableId) noexcept;

}  // namespace stepcompare::gui
