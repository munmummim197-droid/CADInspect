#include <stepcompare/viewer/deviation_coloring.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace stepcompare::viewer {
namespace {

struct ColorStop final {
    double position;
    RgbColor color;
};

constexpr std::array kColorStops{
    ColorStop{0.00, {0.05, 0.20, 0.85}},
    ColorStop{0.25, {0.00, 0.80, 1.00}},
    ColorStop{0.50, {0.10, 0.85, 0.20}},
    ColorStop{0.75, {1.00, 0.85, 0.00}},
    ColorStop{1.00, {0.90, 0.05, 0.02}},
};

[[nodiscard]] bool validScalar(const double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

[[nodiscard]] RgbColor interpolate(const RgbColor& from,
                                   const RgbColor& to,
                                   const double fraction) noexcept {
    return {
        .red = std::lerp(from.red, to.red, fraction),
        .green = std::lerp(from.green, to.green, fraction),
        .blue = std::lerp(from.blue, to.blue, fraction),
    };
}

[[nodiscard]] RgbColor colorAt(const double severity) noexcept {
    const double clamped = std::clamp(severity, 0.0, 1.0);
    for (std::size_t index = 1; index < kColorStops.size(); ++index) {
        const auto& lower = kColorStops[index - 1];
        const auto& upper = kColorStops[index];
        if (clamped <= upper.position) {
            const double fraction =
                (clamped - lower.position) / (upper.position - lower.position);
            return interpolate(lower.color, upper.color, fraction);
        }
    }
    return kColorStops.back().color;
}

}  // namespace

std::optional<DeviationColorScale> makeDeviationColorScale(
    const double maximumObservedMm,
    const double toleranceMm) noexcept {
    if (!validScalar(maximumObservedMm) || !validScalar(toleranceMm)) {
        return std::nullopt;
    }
    return DeviationColorScale{
        .toleranceMm = toleranceMm,
        .maximumMm = std::max(maximumObservedMm, toleranceMm),
    };
}

std::optional<DeviationColorScale> makeDeviationColorScale(
    const std::span<const double> deviationsMm,
    const double toleranceMm) noexcept {
    if (deviationsMm.empty() || !validScalar(toleranceMm)) {
        return std::nullopt;
    }
    double maximumObservedMm = 0.0;
    for (const double deviationMm : deviationsMm) {
        if (!validScalar(deviationMm)) {
            return std::nullopt;
        }
        maximumObservedMm = std::max(maximumObservedMm, deviationMm);
    }
    return makeDeviationColorScale(maximumObservedMm, toleranceMm);
}

std::optional<DeviationColor> mapDeviationToColor(
    const double deviationMm,
    const DeviationColorScale& scale) noexcept {
    if (!validScalar(deviationMm) || !validScalar(scale.toleranceMm) ||
        !validScalar(scale.maximumMm) || scale.maximumMm < scale.toleranceMm) {
        return std::nullopt;
    }

    double severity = 0.0;
    if (deviationMm <= scale.toleranceMm && scale.toleranceMm > 0.0) {
        // Values within tolerance occupy the cool half of the palette.
        severity = 0.5 * deviationMm / scale.toleranceMm;
    } else if (deviationMm == 0.0) {
        severity = 0.0;
    } else if (scale.maximumMm > scale.toleranceMm) {
        severity = 0.5 + 0.5 *
            (deviationMm - scale.toleranceMm) /
            (scale.maximumMm - scale.toleranceMm);
    } else {
        // Zero tolerance with a zero-sized observed range: any positive value
        // remains fail-closed and is shown at maximum severity.
        severity = 1.0;
    }
    severity = std::clamp(severity, 0.0, 1.0);

    return DeviationColor{
        .deviationMm = deviationMm,
        .normalizedSeverity = severity,
        .aboveTolerance = deviationMm > scale.toleranceMm,
        .rgb = colorAt(severity),
    };
}

}  // namespace stepcompare::viewer
