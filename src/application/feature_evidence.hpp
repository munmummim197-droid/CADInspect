#pragma once

#include <stepcompare/assembly/assembly_index.hpp>
#include <stepcompare/deep/deep_geometry_port.hpp>
#include <stepcompare/feature/feature_recognition_port.hpp>
#include <stepcompare/reporting/report.hpp>
#include <stepcompare/domain/types.hpp>

#include <stop_token>

namespace stepcompare::application {

enum class FeatureEvidenceStatus {
    Completed,
    Cancelled,
};

[[nodiscard]] FeatureEvidenceStatus appendFeatureEvidence(
    const assembly::AssemblyIndex& indexA,
    const assembly::AssemblyIndex& indexB,
    const domain::ToleranceSet& tolerances,
    deep::DeepGeometryPort& deepGeometry,
    feature::FeatureRecognitionPort& recognizer,
    bool exactIdentityProven,
    std::stop_token cancellation,
    reporting::Report& report) noexcept;

}  // namespace stepcompare::application
