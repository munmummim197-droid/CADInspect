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

}  // namespace

int main() {
    verdictTests();
    transformTests();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All domain tests passed\n";
    return EXIT_SUCCESS;
}

