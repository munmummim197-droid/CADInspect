#pragma once

#include <optional>
#include <span>

namespace stepcompare::viewer {

struct RgbColor final {
    double red{};
    double green{};
    double blue{};

    friend bool operator==(const RgbColor&, const RgbColor&) = default;
};

// A single, explicit scale is shared by every colored presentation in a scene.
// This prevents equal deviations from being rendered with different colors.
struct DeviationColorScale final {
    double toleranceMm{};
    double maximumMm{};

    friend bool operator==(const DeviationColorScale&,
                           const DeviationColorScale&) = default;
};

struct DeviationColor final {
    double deviationMm{};
    double normalizedSeverity{};
    bool aboveTolerance{};
    RgbColor rgb{};
};

// Invalid or non-finite input is rejected instead of being clamped into a
// misleading low-deviation color.
[[nodiscard]] std::optional<DeviationColorScale> makeDeviationColorScale(
    double maximumObservedMm,
    double toleranceMm) noexcept;

[[nodiscard]] std::optional<DeviationColorScale> makeDeviationColorScale(
    std::span<const double> deviationsMm,
    double toleranceMm) noexcept;

[[nodiscard]] std::optional<DeviationColor> mapDeviationToColor(
    double deviationMm,
    const DeviationColorScale& scale) noexcept;

}  // namespace stepcompare::viewer
