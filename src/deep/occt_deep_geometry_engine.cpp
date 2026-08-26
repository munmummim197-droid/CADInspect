#include "stepcompare/deep/occt_deep_geometry_engine.hpp"

#include "adapters/occt/occt_geometry_payload.hpp"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <GProp_PrincipalProps.hxx>
#include <NCollection_List.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace stepcompare::deep {
namespace {

using Matrix3 = std::array<std::array<double, 3>, 3>;

struct MassFrame final {
    double volume{};
    gp_Pnt center;
    std::array<double, 3> moments{};
    std::array<gp_Vec, 3> axes{};
};

struct Candidate final {
    gp_Trsf transform;
    double commonVolume{};
    double differenceVolume{std::numeric_limits<double>::infinity()};
    double rotationScore{-std::numeric_limits<double>::infinity()};
};

void diagnostic(DeepGeometryResult& result,
                DeepDiagnosticCode code,
                std::string message) {
    result.diagnostics.push_back({code, std::move(message)});
}

bool hasSolid(const TopoDS_Shape& shape) {
    return TopExp_Explorer(shape, TopAbs_SOLID).More();
}

bool isSupportedClosedSolid(const TopoDS_Shape& shape) {
    return !shape.IsNull() && hasSolid(shape) &&
           BRepCheck_Analyzer(shape, true).IsValid();
}

MassFrame massFrame(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties, true, true, false);
    MassFrame frame;
    frame.volume = std::abs(properties.Mass());
    frame.center = properties.CentreOfMass();
    const GProp_PrincipalProps principal = properties.PrincipalProperties();
    principal.Moments(frame.moments[0], frame.moments[1], frame.moments[2]);
    frame.axes = {
        principal.FirstAxisOfInertia(),
        principal.SecondAxisOfInertia(),
        principal.ThirdAxisOfInertia(),
    };
    return frame;
}

bool hasRepeatedMoments(const MassFrame& frame, double tolerance) {
    const double scale = std::max({std::abs(frame.moments[0]),
                                   std::abs(frame.moments[1]),
                                   std::abs(frame.moments[2]),
                                   1.0});
    for (std::size_t first = 0; first < frame.moments.size(); ++first) {
        for (std::size_t second = first + 1U;
             second < frame.moments.size();
             ++second) {
            if (std::abs(frame.moments[first] - frame.moments[second]) <=
                tolerance * scale) {
                return true;
            }
        }
    }
    return false;
}

double determinant(const Matrix3& matrix) noexcept {
    return matrix[0][0] *
               (matrix[1][1] * matrix[2][2] -
                matrix[1][2] * matrix[2][1]) -
           matrix[0][1] *
               (matrix[1][0] * matrix[2][2] -
                matrix[1][2] * matrix[2][0]) +
           matrix[0][2] *
               (matrix[1][0] * matrix[2][1] -
                matrix[1][1] * matrix[2][0]);
}

Matrix3 rotationBetweenFrames(
    const MassFrame& frameA,
    const MassFrame& frameB,
    const std::array<int, 3>& permutation,
    const std::array<double, 3>& signs) {
    Matrix3 rotation{};
    for (std::size_t targetAxis = 0; targetAxis < 3U; ++targetAxis) {
        const gp_Vec& axisA = frameA.axes[targetAxis];
        const gp_Vec& axisB =
            frameB.axes[static_cast<std::size_t>(permutation[targetAxis])];
        const std::array<double, 3> a{axisA.X(), axisA.Y(), axisA.Z()};
        const std::array<double, 3> b{axisB.X(), axisB.Y(), axisB.Z()};
        for (std::size_t row = 0; row < 3U; ++row) {
            for (std::size_t column = 0; column < 3U; ++column) {
                rotation[row][column] +=
                    signs[targetAxis] * a[row] * b[column];
            }
        }
    }
    return rotation;
}

gp_Trsf rigidTransform(const Matrix3& rotation,
                       const gp_Pnt& centerB,
                       const gp_Pnt& centerA) {
    const double translatedX =
        centerA.X() - rotation[0][0] * centerB.X() -
        rotation[0][1] * centerB.Y() - rotation[0][2] * centerB.Z();
    const double translatedY =
        centerA.Y() - rotation[1][0] * centerB.X() -
        rotation[1][1] * centerB.Y() - rotation[1][2] * centerB.Z();
    const double translatedZ =
        centerA.Z() - rotation[2][0] * centerB.X() -
        rotation[2][1] * centerB.Y() - rotation[2][2] * centerB.Z();

    gp_Trsf transform;
    transform.SetValues(rotation[0][0],
                        rotation[0][1],
                        rotation[0][2],
                        translatedX,
                        rotation[1][0],
                        rotation[1][1],
                        rotation[1][2],
                        translatedY,
                        rotation[2][0],
                        rotation[2][1],
                        rotation[2][2],
                        translatedZ);
    return transform;
}

double shapeVolume(const TopoDS_Shape& shape) {
    if (shape.IsNull()) {
        return 0.0;
    }
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties, true, true, false);
    return std::abs(properties.Mass());
}

double commonVolume(const TopoDS_Shape& shapeA,
                    const TopoDS_Shape& alignedB,
                    double fuzzyMm) {
    NCollection_List<TopoDS_Shape> arguments;
    arguments.Append(shapeA);
    NCollection_List<TopoDS_Shape> tools;
    tools.Append(alignedB);
    BRepAlgoAPI_Common common;
    common.SetArguments(arguments);
    common.SetTools(tools);
    common.SetFuzzyValue(fuzzyMm);
    common.SetRunParallel(false);
    common.Build();
    if (common.HasErrors()) {
        throw std::runtime_error("OCCT Boolean Common reported an error");
    }
    return shapeVolume(common.Shape());
}

std::vector<gp_Trsf> alignmentCandidates(const MassFrame& frameA,
                                         const MassFrame& frameB) {
    std::vector<gp_Trsf> candidates;
    std::array<int, 3> permutation{0, 1, 2};
    do {
        for (int mask = 0; mask < 8; ++mask) {
            const std::array<double, 3> signs{
                (mask & 1) != 0 ? -1.0 : 1.0,
                (mask & 2) != 0 ? -1.0 : 1.0,
                (mask & 4) != 0 ? -1.0 : 1.0,
            };
            const Matrix3 rotation =
                rotationBetweenFrames(frameA, frameB, permutation, signs);
            if (determinant(rotation) <= 0.0) {
                continue;
            }
            candidates.push_back(
                rigidTransform(rotation, frameB.center, frameA.center));
        }
    } while (std::next_permutation(permutation.begin(), permutation.end()));
    return candidates;
}

import::RigidTransformMm toContractTransform(const gp_Trsf& transform) {
    import::RigidTransformMm result;
    for (int row = 1; row <= 3; ++row) {
        for (int column = 1; column <= 4; ++column) {
            result.matrix[static_cast<std::size_t>((row - 1) * 4 +
                                                   (column - 1))] =
                transform.Value(row, column);
        }
    }
    return result;
}

DeepGeometryResult compareImpl(const DeepGeometryRequest& request) {
    DeepGeometryResult result;
    if (!std::isfinite(request.options.booleanFuzzyMm) ||
        request.options.booleanFuzzyMm < 0.0 ||
        !std::isfinite(request.options.relativeVolumeTolerance) ||
        request.options.relativeVolumeTolerance < 0.0) {
        diagnostic(result,
                   DeepDiagnosticCode::InvalidTolerance,
                   "Deep geometry tolerances must be finite and non-negative");
        return result;
    }

    const TopoDS_Shape* shapeA = adapters::occt::tryGetShape(request.geometryA);
    const TopoDS_Shape* shapeB = adapters::occt::tryGetShape(request.geometryB);
    if (shapeA == nullptr || shapeB == nullptr) {
        diagnostic(result,
                   DeepDiagnosticCode::MissingGeometryPayload,
                   "Deep geometry comparison requires two OCCT payloads");
        return result;
    }

    if (!hasSolid(*shapeA) || !hasSolid(*shapeB)) {
        result.status = DeepGeometryStatus::OpenShellUnsupported;
        diagnostic(result,
                   DeepDiagnosticCode::InvalidClosedSolid,
                   "Boolean volume comparison requires closed solids");
        return result;
    }
    if (!isSupportedClosedSolid(*shapeA) || !isSupportedClosedSolid(*shapeB)) {
        diagnostic(result,
                   DeepDiagnosticCode::InvalidClosedSolid,
                   "One or both closed-solid payloads are invalid");
        return result;
    }

    const MassFrame frameA = massFrame(*shapeA);
    const MassFrame frameB = massFrame(*shapeB);
    result.volumeAMm3 = frameA.volume;
    result.volumeBMm3 = frameB.volume;
    if (frameA.volume <= 0.0 || frameB.volume <= 0.0) {
        diagnostic(result,
                   DeepDiagnosticCode::InvalidClosedSolid,
                   "Closed-solid volume must be positive");
        return result;
    }

    constexpr double kRepeatedMomentRelativeTolerance = 1.0e-8;
    if (hasRepeatedMoments(frameA, kRepeatedMomentRelativeTolerance) ||
        hasRepeatedMoments(frameB, kRepeatedMomentRelativeTolerance)) {
        result.status = DeepGeometryStatus::AlignmentNotProven;
        diagnostic(result,
                   DeepDiagnosticCode::AlignmentNotProven,
                   "Principal inertia frame is ambiguous by symmetry");
        return result;
    }

    Candidate best;
    bool completedBoolean = false;
    for (const gp_Trsf& transform : alignmentCandidates(frameA, frameB)) {
        try {
            const TopoDS_Shape alignedB =
                BRepBuilderAPI_Transform(*shapeB, transform, true).Shape();
            const double common = commonVolume(
                *shapeA, alignedB, request.options.booleanFuzzyMm);
            const double difference = std::max(
                0.0, frameA.volume + frameB.volume - 2.0 * common);
            const double rotationScore = transform.Value(1, 1) +
                                         transform.Value(2, 2) +
                                         transform.Value(3, 3);
            if (!completedBoolean || difference < best.differenceVolume ||
                (std::abs(difference - best.differenceVolume) <= 1.0e-10 &&
                 rotationScore > best.rotationScore)) {
                best = {transform, common, difference, rotationScore};
            }
            completedBoolean = true;
        } catch (const Standard_Failure&) {
            // A failed candidate is not fatal if another unambiguous frame
            // candidate completes successfully.
        } catch (const std::exception&) {
        }
    }

    if (!completedBoolean) {
        diagnostic(result,
                   DeepDiagnosticCode::BooleanOperationFailed,
                   "OCCT Boolean Common failed for every alignment candidate");
        return result;
    }

    result.transformBToA = toContractTransform(best.transform);
    result.commonVolumeMm3 = best.commonVolume;
    result.symmetricDifferenceVolumeMm3 = best.differenceVolume;
    result.alignmentProven = true;
    const double volumeScale =
        std::max({frameA.volume, frameB.volume, 1.0});
    result.status = best.differenceVolume <=
                            request.options.relativeVolumeTolerance * volumeScale
                        ? DeepGeometryStatus::SameGeometry
                        : DeepGeometryStatus::GeometryChanged;
    return result;
}

}  // namespace

DeepGeometryResult OcctDeepGeometryEngine::compareAligned(
    const DeepGeometryRequest& request) noexcept {
    try {
        return compareImpl(request);
    } catch (const Standard_Failure& failure) {
        DeepGeometryResult result;
        diagnostic(result,
                   DeepDiagnosticCode::OcctFailure,
                   failure.what() != nullptr ? failure.what()
                                             : "Unspecified OCCT failure");
        return result;
    } catch (const std::exception& failure) {
        DeepGeometryResult result;
        diagnostic(result,
                   DeepDiagnosticCode::UnexpectedFailure,
                   failure.what());
        return result;
    } catch (...) {
        DeepGeometryResult result;
        diagnostic(result,
                   DeepDiagnosticCode::UnexpectedFailure,
                   "Unknown failure in deep geometry adapter");
        return result;
    }
}

}  // namespace stepcompare::deep
