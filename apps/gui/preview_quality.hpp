#pragma once

#include <cstddef>
#include <cstdint>

#include <stepcompare/domain/geometry.hpp>
#include <stepcompare/import/imported_model.hpp>

namespace stepcompare::gui {

enum class PreviewQualityTier {
    Detail,
    Balanced,
    Scalable,
};

struct PreviewQualityPolicy final {
    PreviewQualityTier tier{PreviewQualityTier::Detail};
    double relativeLinearDeflection{0.00075};
    double minimumLinearDeflectionMm{0.01};
    double maximumLinearDeflectionMm{0.20};
    double angularDeflectionDegrees{8.0};
    bool drawFeatureEdges{true};
};

struct PreviewMeshSummary final {
    PreviewQualityPolicy policy{};
    std::size_t occurrenceCount{};
    std::size_t prototypeCount{};
    std::size_t meshedPrototypeCount{};
    std::size_t failedPrototypeCount{};
    std::size_t reusedOccurrenceCount{};
    std::uint64_t triangleCount{};
    double elapsedMilliseconds{};
};

[[nodiscard]] PreviewQualityPolicy choosePreviewQuality(
    std::size_t occurrenceCount,
    std::size_t prototypeCount) noexcept;

[[nodiscard]] double previewLinearDeflectionMm(
    const stepcompare::domain::GeometryStatistics& statistics,
    const PreviewQualityPolicy& policy) noexcept;

// Runs only on the preview worker's private ImportedModel before the model is
// handed to the GUI thread. Each prototype is tessellated once; repeated
// occurrences reuse the triangulation through OCCT's shared TShape.
[[nodiscard]] PreviewMeshSummary preparePreviewMeshes(
    stepcompare::import::ImportedModel& model) noexcept;

[[nodiscard]] const char* previewQualityTierName(
    PreviewQualityTier tier) noexcept;

}  // namespace stepcompare::gui
