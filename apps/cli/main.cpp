#include <stepcompare/cli/arguments.hpp>
#include <stepcompare/cli/exit_codes.hpp>

#ifdef STEPCOMPARE_CLI_HAS_COMPOSITION
#include <stepcompare/application/comparison_coordinator.hpp>
#include <stepcompare/cache/file_identity.hpp>
#include <stepcompare/deep/occt_deep_geometry_engine.hpp>
#include <stepcompare/deviation/occt_surface_deviation_engine.hpp>
#include <stepcompare/import/occt_step_importer.hpp>
#include <stepcompare/reporting/writers.hpp>
#endif

#include <filesystem>
#include <fstream>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

#ifdef STEPCOMPARE_CLI_HAS_COMPOSITION
[[nodiscard]] std::u8string asUtf8(std::string_view value) {
    return {reinterpret_cast<const char8_t*>(value.data()), value.size()};
}

[[nodiscard]] std::filesystem::path pathFromUtf8(std::string_view value) {
    return std::filesystem::path(asUtf8(value));
}

void enrichIdentity(std::string_view pathUtf8,
                    stepcompare::reporting::InputIdentity& reportIdentity) {
    const auto identity =
        stepcompare::cache::computeFileIdentity(pathFromUtf8(pathUtf8));
    if (!identity) {
        return;
    }
    reportIdentity.sha256 = identity->sha256Hex;
    reportIdentity.sizeBytes =
        static_cast<std::uint64_t>(identity->sizeBytes);
    // FileIdentity retains exact raw time for cache keys. Report UTC text stays
    // empty until a canonical UTC formatter is available; never fake it.
}

[[nodiscard]] bool writeReportFile(
    std::string_view pathUtf8,
    const stepcompare::reporting::Report& report,
    bool json) {
    try {
        std::ofstream output(pathFromUtf8(pathUtf8),
                             std::ios::binary | std::ios::trunc);
        if (!output) {
            return false;
        }
        if (json) {
            stepcompare::reporting::writeJson(report, output);
        } else {
            stepcompare::reporting::writeCsv(report, output);
        }
        return static_cast<bool>(output);
    } catch (...) {
        return false;
    }
}

[[nodiscard]] int outcomeFor(
    const stepcompare::application::ComparisonResult& result) {
    using stepcompare::application::ComparisonRunStatus;
    using stepcompare::cli::ProcessOutcome;
    using stepcompare::domain::Decision;
    if (result.status == ComparisonRunStatus::Cancelled) {
        return stepcompare::cli::exitCode(ProcessOutcome::Cancelled);
    }
    if (result.status == ComparisonRunStatus::InputError) {
        return stepcompare::cli::exitCode(ProcessOutcome::InputFailure);
    }
    if (result.status == ComparisonRunStatus::ProcessingError) {
        return stepcompare::cli::exitCode(ProcessOutcome::ProcessingFailure);
    }
    switch (result.verdict.decision) {
    case Decision::Pass:
        return stepcompare::cli::exitCode(ProcessOutcome::Pass);
    case Decision::Fail:
        return stepcompare::cli::exitCode(ProcessOutcome::DifferencesFound);
    case Decision::Check:
        return stepcompare::cli::exitCode(ProcessOutcome::CheckRequired);
    case Decision::Error:
        return stepcompare::cli::exitCode(ProcessOutcome::ProcessingFailure);
    }
    return stepcompare::cli::exitCode(ProcessOutcome::ProcessingFailure);
}
#endif

int run(std::span<const std::string_view> arguments) {
    const auto parsed = stepcompare::cli::parseArguments(arguments);
    if (!parsed) {
        std::cerr << "Argument error: " << parsed.message << '\n'
                  << "Usage: stepcompare-cli A.step B.step "
                     "[--linear-tol mm] [--angular-tol degrees] [--deep] "
                     "[--json path] [--csv path]\n";
        return stepcompare::cli::exitCode(
            stepcompare::cli::ProcessOutcome::InvalidArguments);
    }

#ifdef STEPCOMPARE_CLI_HAS_COMPOSITION
    stepcompare::import::OcctStepImporter importer;
    stepcompare::deep::OcctDeepGeometryEngine deepGeometry;
    stepcompare::deviation::OcctSurfaceDeviationEngine surfaceDeviation;
    stepcompare::application::ComparisonCoordinator coordinator(
        importer, deepGeometry, &surfaceDeviation);

    stepcompare::application::ComparisonRequest request;
    request.inputAUtf8 = asUtf8(parsed.options->inputAUtf8);
    request.inputBUtf8 = asUtf8(parsed.options->inputBUtf8);
    request.tolerances.positionMm = parsed.options->linearToleranceMm;
    request.tolerances.surfaceMm = parsed.options->linearToleranceMm;
    request.tolerances.angularDegrees =
        parsed.options->angularToleranceDegrees;
    request.deep = parsed.options->deep;
    request.identityA = stepcompare::cache::computeFileIdentity(
        pathFromUtf8(parsed.options->inputAUtf8));
    request.identityB = stepcompare::cache::computeFileIdentity(
        pathFromUtf8(parsed.options->inputBUtf8));
    auto result = coordinator.compare(request);

    enrichIdentity(parsed.options->inputAUtf8, result.report.inputA);
    enrichIdentity(parsed.options->inputBUtf8, result.report.inputB);

    bool reportWriteFailed = false;
    if (parsed.options->jsonOutputUtf8) {
        reportWriteFailed |= !writeReportFile(
            *parsed.options->jsonOutputUtf8, result.report, true);
    }
    if (parsed.options->csvOutputUtf8) {
        reportWriteFailed |= !writeReportFile(
            *parsed.options->csvOutputUtf8, result.report, false);
    }
    for (const auto& diagnostic : result.diagnostics) {
        std::cerr << "Diagnostic: " << diagnostic.messageUtf8 << '\n';
    }
    std::cout << result.report.verdict.decision;
    for (const auto& reason : result.report.verdict.reasons) {
        std::cout << ' ' << reason;
    }
    std::cout << '\n';
    if (reportWriteFailed) {
        std::cerr << "Report error: unable to write one or more requested "
                     "report files.\n";
        return stepcompare::cli::exitCode(
            stepcompare::cli::ProcessOutcome::ProcessingFailure);
    }
    return outcomeFor(result);
#else
    std::cerr << "CHECK: comparison processing is unavailable because no "
                 "comparison coordinator is connected; no reports were written.\n";
    return stepcompare::cli::exitCode(
        stepcompare::cli::ProcessOutcome::CheckRequired);
#endif
}

#ifdef _WIN32
[[nodiscard]] bool appendUtf8(std::wstring_view value, std::string& output) {
    if (value.empty()) {
        output.clear();
        return true;
    }
    const auto required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return false;
    }
    output.resize(static_cast<std::size_t>(required));
    const auto converted = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        output.data(), required, nullptr, nullptr);
    return converted == required;
}
#endif

}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::string> storage;
    storage.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
    for (int index = 1; index < argc; ++index) {
        std::string converted;
        if (!appendUtf8(argv[index], converted)) {
            std::cerr << "Argument error: Windows argument conversion to UTF-8 "
                         "failed.\n";
            return stepcompare::cli::exitCode(
                stepcompare::cli::ProcessOutcome::InvalidArguments);
        }
        storage.push_back(std::move(converted));
    }
    std::vector<std::string_view> arguments;
    arguments.reserve(storage.size());
    for (const auto& argument : storage) {
        arguments.emplace_back(argument);
    }
    return run(arguments);
}
#else
int main(int argc, char* argv[]) {
    std::vector<std::string_view> arguments;
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return run(arguments);
}
#endif
