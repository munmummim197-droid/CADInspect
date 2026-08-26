#include <stepcompare/cli/arguments.hpp>
#include <stepcompare/cli/exit_codes.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

template <std::size_t Size>
stepcompare::cli::ParseResult parse(
    const std::array<std::string_view, Size>& arguments) {
    return stepcompare::cli::parseArguments(arguments);
}

void completeCommandTests() {
    constexpr std::array arguments{
        std::string_view{"D:\\Dự án\\Chi tiết A.STEP"},
        std::string_view{"D:\\Dự án\\Chi tiết B.stp"},
        std::string_view{"--linear-tol"}, std::string_view{"0.025"},
        std::string_view{"--angular-tol"}, std::string_view{"0.5"},
        std::string_view{"--deep"}, std::string_view{"--json"},
        std::string_view{"D:\\Kết quả\\báo cáo.json"},
        std::string_view{"--csv"},
        std::string_view{"D:\\Kết quả\\báo cáo.csv"},
    };
    const auto result = parse(arguments);
    expect(static_cast<bool>(result), "complete CLI command must parse");
    if (!result) {
        return;
    }
    expect(result.options->inputAUtf8 == arguments[0],
           "input A UTF-8 bytes must be preserved");
    expect(result.options->inputBUtf8 == arguments[1],
           "input B UTF-8 bytes must be preserved");
    expect(std::abs(result.options->linearToleranceMm - 0.025) < 1.0e-15,
           "linear tolerance must parse locale-independently");
    expect(std::abs(result.options->angularToleranceDegrees - 0.5) < 1.0e-15,
           "angular tolerance must parse");
    expect(result.options->deep, "--deep must enable deep comparison");
    expect(result.options->jsonOutputUtf8 == arguments[8],
           "--json path must be preserved as UTF-8");
    expect(result.options->csvOutputUtf8 == arguments[10],
           "--csv path must be preserved as UTF-8");
}

void defaultAndOrderingTests() {
    constexpr std::array minimal{
        std::string_view{"A.step"}, std::string_view{"B.STP"}};
    const auto defaults = parse(minimal);
    expect(static_cast<bool>(defaults), "two supported inputs must parse");
    if (defaults) {
        expect(defaults.options->linearToleranceMm == 0.01,
               "default linear tolerance must be 0.01 mm");
        expect(defaults.options->angularToleranceDegrees == 0.01,
               "default angular tolerance must be 0.01 degrees");
        expect(!defaults.options->deep, "deep comparison defaults off");
    }

    constexpr std::array interleaved{
        std::string_view{"--deep"}, std::string_view{"A.step"},
        std::string_view{"--linear-tol"}, std::string_view{"1e-3"},
        std::string_view{"B.stp"}};
    expect(static_cast<bool>(parse(interleaved)),
           "options may be interleaved with positional inputs");
}

void validationTests() {
    using stepcompare::cli::ArgumentErrorCode;

    constexpr std::array missing{std::string_view{"A.step"}};
    expect(parse(missing).errorCode == ArgumentErrorCode::MissingInput,
           "two input files are required");

    constexpr std::array extra{std::string_view{"A.step"},
                               std::string_view{"B.step"},
                               std::string_view{"C.step"}};
    expect(parse(extra).errorCode == ArgumentErrorCode::TooManyInputs,
           "a third positional input must be rejected");

    constexpr std::array extension{std::string_view{"A.iges"},
                                   std::string_view{"B.step"}};
    expect(parse(extension).errorCode ==
               ArgumentErrorCode::UnsupportedInputExtension,
           "only .step and .stp inputs are accepted");

    constexpr std::array zero{std::string_view{"A.step"},
                              std::string_view{"B.step"},
                              std::string_view{"--linear-tol"},
                              std::string_view{"0"}};
    expect(parse(zero).errorCode == ArgumentErrorCode::InvalidTolerance,
           "zero tolerance must be rejected");

    constexpr std::array negative{std::string_view{"A.step"},
                                  std::string_view{"B.step"},
                                  std::string_view{"--angular-tol"},
                                  std::string_view{"-0.1"}};
    expect(parse(negative).errorCode == ArgumentErrorCode::InvalidTolerance,
           "negative tolerance must be rejected");

    constexpr std::array commaDecimal{std::string_view{"A.step"},
                                      std::string_view{"B.step"},
                                      std::string_view{"--linear-tol"},
                                      std::string_view{"0,01"}};
    expect(parse(commaDecimal).errorCode == ArgumentErrorCode::InvalidTolerance,
           "comma decimal syntax must be rejected consistently");

    constexpr std::array nonFinite{std::string_view{"A.step"},
                                   std::string_view{"B.step"},
                                   std::string_view{"--linear-tol"},
                                   std::string_view{"nan"}};
    expect(parse(nonFinite).errorCode == ArgumentErrorCode::InvalidTolerance,
           "non-finite tolerance must be rejected");

    constexpr std::array trailingText{std::string_view{"A.step"},
                                      std::string_view{"B.step"},
                                      std::string_view{"--linear-tol"},
                                      std::string_view{"0.01mm"}};
    expect(parse(trailingText).errorCode == ArgumentErrorCode::InvalidTolerance,
           "tolerance values must be consumed completely");

    constexpr std::array missingValue{std::string_view{"A.step"},
                                      std::string_view{"B.step"},
                                      std::string_view{"--json"}};
    expect(parse(missingValue).errorCode == ArgumentErrorCode::MissingOptionValue,
           "output options require a path");

    constexpr std::array unknown{std::string_view{"A.step"},
                                 std::string_view{"B.step"},
                                 std::string_view{"--fastest"}};
    expect(parse(unknown).errorCode == ArgumentErrorCode::UnknownOption,
           "unknown options must fail closed");

    constexpr std::array duplicate{std::string_view{"A.step"},
                                   std::string_view{"B.step"},
                                   std::string_view{"--deep"},
                                   std::string_view{"--deep"}};
    expect(parse(duplicate).errorCode == ArgumentErrorCode::DuplicateOption,
           "duplicate options must be rejected");

    constexpr char invalidUtf8Bytes[]{static_cast<char>(0xc3),
                                      static_cast<char>(0x28), '\0'};
    const std::array invalidUtf8{std::string_view{invalidUtf8Bytes, 2},
                                std::string_view{"B.step"}};
    expect(parse(invalidUtf8).errorCode == ArgumentErrorCode::InvalidUtf8,
           "malformed UTF-8 must be rejected at the CLI boundary");
}

void exitCodeTests() {
    using namespace stepcompare::cli;
    expect(exitCode(ProcessOutcome::Pass) == 0, "PASS exit code must be 0");
    expect(exitCode(ProcessOutcome::DifferencesFound) == 1,
           "differences exit code must be 1");
    expect(exitCode(ProcessOutcome::CheckRequired) == 2,
           "CHECK exit code must be 2");
    expect(exitCode(ProcessOutcome::InvalidArguments) == 3,
           "argument error exit code must be 3");
    expect(exitCode(ProcessOutcome::InputFailure) == 4,
           "input error exit code must be 4");
    expect(exitCode(ProcessOutcome::ProcessingFailure) == 5,
           "processing error exit code must be 5");
    expect(exitCode(ProcessOutcome::Cancelled) == 130,
           "cancel exit code must be 130");
}

}  // namespace

int main() {
    completeCommandTests();
    defaultAndOrderingTests();
    validationTests();
    exitCodeTests();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All CLI tests passed\n";
    return EXIT_SUCCESS;
}
