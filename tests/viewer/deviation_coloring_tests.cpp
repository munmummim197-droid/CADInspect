#include <stepcompare/viewer/deviation_coloring.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {

int failures = 0;

void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void rejectsInvalidEvidence() {
    using namespace stepcompare::viewer;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    expect(!makeDeviationColorScale(nan, 0.01),
           "non-finite maximum must not produce a heatmap scale");
    expect(!makeDeviationColorScale(1.0, -0.01),
           "negative tolerance must not produce a heatmap scale");

    const std::vector values{0.0, nan, 0.1};
    expect(!makeDeviationColorScale(values, 0.01),
           "one invalid value must reject the complete scale");

    const auto scale = makeDeviationColorScale(1.0, 0.01);
    expect(scale && !mapDeviationToColor(nan, *scale),
           "invalid component evidence must not be colored as low deviation");
}

void preservesToleranceSemantics() {
    using namespace stepcompare::viewer;
    const auto scale = makeDeviationColorScale(1.0, 0.1);
    expect(scale.has_value(), "valid evidence must produce a color scale");
    if (!scale) {
        return;
    }

    const auto zero = mapDeviationToColor(0.0, *scale);
    const auto atTolerance = mapDeviationToColor(0.1, *scale);
    const auto above = mapDeviationToColor(0.100001, *scale);
    const auto maximum = mapDeviationToColor(1.0, *scale);
    expect(zero && !zero->aboveTolerance && zero->normalizedSeverity == 0.0,
           "zero deviation must be cool and within tolerance");
    expect(atTolerance && !atTolerance->aboveTolerance &&
               std::abs(atTolerance->normalizedSeverity - 0.5) < 1e-12,
           "the inclusive tolerance boundary must remain within tolerance");
    expect(above && above->aboveTolerance &&
               above->normalizedSeverity > atTolerance->normalizedSeverity,
           "above-tolerance evidence must use the hot half of the scale");
    expect(maximum && std::abs(maximum->normalizedSeverity - 1.0) < 1e-12,
           "the observed maximum must map to maximum severity");
}

void clampsOutliersWithoutChangingTheirVerdict() {
    using namespace stepcompare::viewer;
    const auto scale = makeDeviationColorScale(1.0, 0.1);
    const auto outlier = mapDeviationToColor(10.0, *scale);
    expect(outlier && outlier->aboveTolerance,
           "outlier remains classified above tolerance");
    expect(outlier && outlier->normalizedSeverity == 1.0,
           "outlier color must clamp to the hot endpoint");
}

void usesOneScaleForACollection() {
    using namespace stepcompare::viewer;
    const std::vector values{0.0, 0.05, 2.0, 0.5};
    const auto scale = makeDeviationColorScale(values, 0.1);
    expect(scale && scale->maximumMm == 2.0,
           "collection scale must use the actual maximum evidence");
    if (!scale) {
        return;
    }
    const auto low = mapDeviationToColor(0.05, *scale);
    const auto high = mapDeviationToColor(0.5, *scale);
    expect(low && high && low->normalizedSeverity < high->normalizedSeverity,
           "greater deviation must not appear cooler on a shared scale");
}

}  // namespace

int main() {
    rejectsInvalidEvidence();
    preservesToleranceSemantics();
    clampsOutliersWithoutChangingTheirVerdict();
    usesOneScaleForACollection();

    if (failures != 0) {
        std::cerr << failures << " deviation coloring assertion(s) failed\n";
        return 1;
    }
    std::cout << "All deviation coloring tests passed\n";
    return 0;
}
