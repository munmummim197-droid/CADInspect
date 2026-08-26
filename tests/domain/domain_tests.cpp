#include <stepcompare/domain/fast_check.hpp>
#include <stepcompare/domain/placement.hpp>
#include <stepcompare/domain/result.hpp>
#include <stepcompare/domain/types.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <string_view>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void verdictTests() {
    using namespace stepcompare::domain;

    const auto exact = reduceVerdict({
        .geometry = GeometryStatus::SameProven,
        .position = PositionStatus::Same,
        .allRequiredStagesComplete = true,
    });
    expect(exact.decision == Decision::Pass,
           "proven same geometry and position must PASS");
    expect(exact.reasons.size() == 1 &&
               exact.reasons.front() == ReasonCode::SameGeometrySamePosition,
           "PASS must carry SAME_GEOMETRY_SAME_POSITION");

    const auto moved = reduceVerdict({
        .geometry = GeometryStatus::SameProven,
        .position = PositionStatus::Translated,
        .allRequiredStagesComplete = true,
    });
    expect(moved.decision == Decision::Fail,
           "same geometry at a translated position must FAIL");

    const auto probable = reduceVerdict({
        .geometry = GeometryStatus::ProbableSame,
        .position = PositionStatus::Same,
        .allRequiredStagesComplete = true,
    });
    expect(probable.decision == Decision::Check,
           "probable geometry must fail closed to CHECK");

    const auto changed = reduceVerdict({
        .geometry = GeometryStatus::ChangedProven,
        .position = PositionStatus::Same,
        .allRequiredStagesComplete = true,
    });
    expect(changed.decision == Decision::Fail,
           "proven geometry change must FAIL");

    EvidenceSummary importFailure{};
    importFailure.stepImportFailed = true;
    const auto importError = reduceVerdict(importFailure);
    expect(importError.decision == Decision::Error,
           "STEP import failure must be ERROR");

    const auto symmetric = reduceVerdict({
        .geometry = GeometryStatus::SameProven,
        .position = PositionStatus::Unknown,
        .allRequiredStagesComplete = true,
        .rotationAmbiguousBySymmetry = true,
    });
    expect(symmetric.decision == Decision::Check,
           "symmetry ambiguity must not create false FAIL");
}

void transformTests() {
    using namespace stepcompare::domain;

    const auto delta = absoluteTranslationBMinusA({1.0, 2.0, 3.0},
                                                  {6.0, 1.99, 3.0});
    expect(std::abs(delta.x - 5.0) < 1.0e-12, "Delta X must be B minus A");
    expect(std::abs(delta.y + 0.01) < 1.0e-12, "Delta Y must be B minus A");
    expect(std::abs(delta.z) < 1.0e-12, "Delta Z must be B minus A");

    constexpr auto halfDegreeRadians = 0.5 * std::numbers::pi / 180.0;
    const Quaternion rotatedAboutZ{
        std::cos(halfDegreeRadians / 2.0),
        0.0,
        0.0,
        std::sin(halfDegreeRadians / 2.0),
    };
    const auto measured = relativeRotationAngleRadians({}, rotatedAboutZ);
    expect(std::abs(measured - halfDegreeRadians) < 1.0e-12,
           "relative rotation must preserve a 0.5 degree Z rotation");

    const Quaternion sameRotationNegated{-rotatedAboutZ.w,
                                         -rotatedAboutZ.x,
                                         -rotatedAboutZ.y,
                                         -rotatedAboutZ.z};
    const auto equivalent = relativeRotationAngleRadians(rotatedAboutZ,
                                                          sameRotationNegated);
    expect(std::abs(equivalent) < 1.0e-12,
           "q and -q must represent the same orientation");
}

stepcompare::domain::GeometryStatistics boxStatistics() {
    using namespace stepcompare::domain;
    return {
        .boundingBox = {{0.0, 0.0, 0.0}, {10.0, 20.0, 30.0}},
        .volumeMm3 = 6000.0,
        .surfaceAreaMm2 = 2200.0,
        .centerOfMassMm = {5.0, 10.0, 15.0},
        .topology = {1, 1, 6, 12, 8},
        .principalInertia = {
            {250000.0, 500000.0, 650000.0},
            {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}},
        },
        .closedSolidEvidence = true,
    };
}

void fastCheckTests() {
    using namespace stepcompare::domain;
    const ToleranceSet tolerances{};
    const auto a = boxStatistics();

    auto moved = a;
    moved.boundingBox.minimum.x += 5.0;
    moved.boundingBox.maximum.x += 5.0;
    moved.centerOfMassMm.x += 5.0;
    const auto movedFast = compareFastInvariants(a, moved, tolerances);
    expect(movedFast.status == FastScreenStatus::CompatibleCandidate,
           "translation must not change invariant fast-screen compatibility");

    auto sameVolumeDifferentTopology = a;
    sameVolumeDifferentTopology.topology.faces = 8;
    const auto topology = compareFastInvariants(
        a, sameVolumeDifferentTopology, tolerances);
    expect(topology.status == FastScreenStatus::Different,
           "same volume with different topology must fast-screen DIFFERENT");

    auto sameBoundingBoxDifferentVolume = a;
    sameBoundingBoxDifferentVolume.volumeMm3 = 5900.0;
    const auto volume = compareFastInvariants(
        a, sameBoundingBoxDifferentVolume, tolerances);
    expect(volume.status == FastScreenStatus::Different,
           "same bounding box with different volume must fast-screen DIFFERENT");
}

void placementAndRotationTests() {
    using namespace stepcompare::domain;
    const ToleranceSet tolerances{};
    const auto a = boxStatistics();
    auto b = a;
    b.boundingBox.minimum.x += 5.0;
    b.boundingBox.maximum.x += 5.0;
    b.centerOfMassMm.x += 5.0;

    const auto moved = analyzeAbsolutePlacement({.a = a, .b = b}, tolerances);
    expect(moved.status == PlacementAnalysisStatus::Translated,
           "consistent +5 mm signals must classify as translated");
    expect(std::abs(moved.deltaBMinusA.x - 5.0) < 1.0e-12,
           "absolute placement Delta X must equal +5 mm");

    const auto conflicting = analyzeAbsolutePlacement(
        {
            .a = a,
            .b = b,
            .componentPositionA = Vec3Mm{0.0, 0.0, 0.0},
            .componentPositionB = Vec3Mm{8.0, 0.0, 0.0},
        },
        tolerances);
    expect(conflicting.status == PlacementAnalysisStatus::ConflictingSignals,
           "disagreeing COM/bbox/component signals must not fake a delta");

    constexpr auto halfDegreeRadians = 0.5 * std::numbers::pi / 180.0;
    const Quaternion rotatedZ{
        std::cos(halfDegreeRadians / 2.0),
        0.0,
        0.0,
        std::sin(halfDegreeRadians / 2.0),
    };
    const auto ordinary = analyzeRotation({}, rotatedZ, {}, tolerances);
    expect(ordinary.status == RotationAnalysisStatus::Rotated,
           "0.5 degree rotation must exceed the 0.01 degree tolerance");

    const auto cylinder = analyzeRotation(
        {}, rotatedZ, {SymmetryKind::Axial, {0.0, 0.0, 1.0}}, tolerances);
    expect(cylinder.status == RotationAnalysisStatus::AmbiguousBySymmetry,
           "cylinder spin around its axis must be symmetry-ambiguous");
}

}  // namespace

int main() {
    verdictTests();
    transformTests();
    fastCheckTests();
    placementAndRotationTests();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All domain tests passed\n";
    return EXIT_SUCCESS;
}
